#include "Mandrill.h"

using namespace Mandrill;

class DSRT : public App
{
public:
	enum {
		GBUFFER_PASS = 0,
		RT_PASS = 1,
		RESOLVE_PASS = 2,
	};

	struct PushConstants {
		glm::vec3 lightPosition;
		float aoDistance;
		int renderMode;
	};

	static std::shared_ptr<Image> createColorAttachmentImage(std::shared_ptr<Device> pDevice, uint32_t width,
		uint32_t height, VkFormat format)
	{
		return pDevice->createImage(width, height, 1, 1, VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	void transitionAttachmentsForGBuffer(VkCommandBuffer cmd)
	{
		for (auto& attachment : mColorAttachments) {
			Helpers::imageBarrier(cmd, attachment->getImage(), VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_GENERAL);
		}
	}

	void transitionAttachmentsForRayTracing(VkCommandBuffer cmd)
	{
		for (auto& attachment : mColorAttachments) {
			Helpers::imageBarrier(cmd, attachment->getImage(), VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
				VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
		}
	}

	void transitionAttachmentsForResolve(VkCommandBuffer cmd)
	{
		for (auto& attachment : mColorAttachments) {
			Helpers::imageBarrier(cmd, attachment->getImage(), VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_WRITE_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
		}
	}

	void createAttachments()
	{
		uint32_t width = mpSwapchain->getExtent().width;
		uint32_t height = mpSwapchain->getExtent().height;
		VkFormat depthFormat = Helpers::findDepthFormat(mpDevice);

		mColorAttachments.clear();
		mColorAttachments.push_back(createColorAttachmentImage(mpDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT));
		mColorAttachments.push_back(createColorAttachmentImage(mpDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT));
		mColorAttachments.push_back(createColorAttachmentImage(mpDevice, width, height, VK_FORMAT_R8G8B8A8_UNORM));
		mColorAttachments.push_back(createColorAttachmentImage(mpDevice, width, height, VK_FORMAT_R8G8B8A8_UNORM));
		mpDepthAttachment =
			mpDevice->createImage(width, height, 1, 1, VK_SAMPLE_COUNT_1_BIT, depthFormat, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Create image views and transition image layouts
		VkCommandBuffer cmd = Helpers::cmdBegin(mpDevice);

		for (auto& attachment : mColorAttachments) {
			attachment->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);

			// Transition to correct layout for descriptor creation
			Helpers::imageBarrier(cmd, attachment->getImage(), VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		}

		// Transition depth attachment for depth use
		VkImageSubresourceRange depthSubresourceRange = { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
														 .baseMipLevel = 0,
														 .levelCount = 1,
														 .baseArrayLayer = 0,
														 .layerCount = 1 };
		if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
			depthSubresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		Helpers::imageBarrier(
			cmd, mpDepthAttachment->getImage(), VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &depthSubresourceRange);

		Helpers::cmdEnd(mpDevice, cmd);
	}

	void createAttachmentDescriptor()
	{
		std::vector<DescriptorDesc> descriptorDesc;
		for (auto& attachment : mColorAttachments) {
			descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, attachment);
			descriptorDesc.back().imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		mpColorAttachmentDescriptor = mpDevice->createDescriptor(
			descriptorDesc, mPipelines[RESOLVE_PASS]->getShader()->getDescriptorSetLayout(0));
	}

	void createRayTracingDescriptor()
	{
		std::vector<DescriptorDesc> descriptorDesc;
		for (auto& attachment : mColorAttachments) {
			descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, attachment);
			descriptorDesc.back().imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		mpRayTracingDescriptor = mpDevice->createDescriptor(
			descriptorDesc, mPipelines[RT_PASS]->getShader()->getDescriptorSetLayout(2));
	}

	DSRT() : App("Deferred Shading and Ray Tracing", 1920, 1080)
	{
		// Create a Vulkan instance and device
		mpDevice = std::make_shared<Device>(mpWindow);

		// Create a swapchain with 2 frames in flight (default)
		mpSwapchain = mpDevice->createSwapchain();

		// Create scene
		mpScene = mpDevice->createScene();

		// Create the attachments
		createAttachments();

		// Setup specialization constants with scene information for ray gen shader
		mSpecializationConstants.resize(5, 1);

		for (uint32_t i = 0; i < mSpecializationConstants.size(); i++) {
			VkSpecializationMapEntry entry = {
				.constantID = i,
				.offset = i * static_cast<uint32_t>(sizeof(uint32_t)),
				.size = static_cast<uint32_t>(sizeof(uint32_t)),
			};
			mSpecializationMapEntries.push_back(entry);
		}

		mSpecializationInfo = {
			.mapEntryCount = count(mSpecializationMapEntries),
			.pMapEntries = mSpecializationMapEntries.data(),
			.dataSize = mSpecializationConstants.size() * sizeof(uint32_t),
			.pData = mSpecializationConstants.data(),
		};

		// Create a passes for GBuffer
		mpGBufferPass = mpDevice->createPass(mColorAttachments, mpDepthAttachment);
		mpResolvePass = mpDevice->createPass(mpSwapchain->getExtent(), mpSwapchain->getImageFormat());

		// Create three shaders (and pipelines) for G-buffer, Ray tracing, and resolve pass respectively
		std::vector<ShaderDesc> shaderDesc;
		shaderDesc.emplace_back("DSRT/GBuffer.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
		shaderDesc.emplace_back("DSRT/GBuffer.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
		auto pGBufferShader = mpDevice->createShader(shaderDesc);

		shaderDesc.clear();
		shaderDesc.emplace_back("DSRT/RayGen.rgen", "main", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		shaderDesc.emplace_back("DSRT/RayMiss.rmiss", "main", VK_SHADER_STAGE_MISS_BIT_KHR);
		shaderDesc.emplace_back("DSRT/RayClosestHit.rchit", "main", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, &mSpecializationInfo);
		auto pRayTracingShader = mpDevice->createShader(shaderDesc);

		shaderDesc.clear();
		shaderDesc.emplace_back("DSRT/Resolve.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
		shaderDesc.emplace_back("DSRT/Resolve.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
		auto pResolveShader = mpDevice->createShader(shaderDesc);

		// Create pipelines
		PipelineDesc pipelineDesc;
		pipelineDesc.depthTestEnable = VK_TRUE;
		mPipelines.emplace_back(mpDevice->createPipeline(mpGBufferPass, pGBufferShader, pipelineDesc));

		RayTracingPipelineDesc rtPipelineDesc(
			1, // missGroupCount
			1, // hitGroupCount
			1  // maxRecursionDepth
		);
		rtPipelineDesc.setRayGen(0);
		rtPipelineDesc.setMissGroup(0, 1);
		rtPipelineDesc.setHitGroup(0, 2);
		mPipelines.emplace_back(mpDevice->createRayTracingPipeline(pRayTracingShader, rtPipelineDesc));

		pipelineDesc.depthTestEnable = VK_FALSE;
		mPipelines.emplace_back(mpDevice->createPipeline(mpResolvePass, pResolveShader, pipelineDesc));

		// Create descriptor for resolve pass input attachments
		createAttachmentDescriptor();

		// Create descriptor for ray tracing pass
		createRayTracingDescriptor();

		// Load scene
		auto meshIndices = mpScene->addMeshFromFile(GetResourcePath("scenes/crytek_sponza/sponza.obj"));
		std::shared_ptr<Node> pNode = mpScene->addNode();
		pNode->setPipeline(mPipelines[GBUFFER_PASS]); // Render scene with first pass pipeline
		for (auto meshIndex : meshIndices) {
			pNode->addMesh(meshIndex);
		}
		// Scale down the model
		pNode->setTransform(glm::scale(glm::vec3(0.01f)));

		mpScene->compile(mpSwapchain->getFramesInFlightCount());
		mpScene->createDescriptors(mPipelines[GBUFFER_PASS]->getShader()->getDescriptorSetLayouts(),
			mpSwapchain->getFramesInFlightCount());
		mpScene->syncToDevice();

		// Set specialization constants now that the scene parameters are calculated
		mSpecializationConstants[0] = mpScene->getVertexCount();   // VERTEX_COUNT
		mSpecializationConstants[1] = mpScene->getIndexCount();    // INDEX_COUNT
		mSpecializationConstants[2] = mpScene->getMaterialCount(); // MATERIAL_COUNT
		mSpecializationConstants[3] = mpScene->getTextureCount();  // TEXTURE_COUNT
		mSpecializationConstants[4] = mpScene->getMeshCount();     // MESH_COUNT
		mPipelines[RT_PASS]->recreate();                           // Rebuild layouts

		// Create acceleration structure
		mpAccelerationStructure =
			mpDevice->createAccelerationStructure(mpScene, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

		// Create descriptors when layouts are defined
		mpScene->createRayTracingDescriptors(pRayTracingShader->getDescriptorSetLayouts(), mpAccelerationStructure,
			mpSwapchain->getFramesInFlightCount());

		// Activate back-face culling for G-buffer pass
		mPipelines[GBUFFER_PASS]->setCullMode(VK_CULL_MODE_BACK_BIT);

		// Setup camera
		mpCamera = mpDevice->createCamera(mpWindow, mpSwapchain);
		mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
		mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
		mpCamera->setFov(60.0f);
		mpCamera->createDescriptor(VK_SHADER_STAGE_VERTEX_BIT);
		mpCamera->createRayTracingDescriptor(VK_SHADER_STAGE_RAYGEN_BIT_KHR);

		// Initialize GUI
		App::createGUI(mpDevice, mpResolvePass);
	}

	~DSRT()
	{
		App::destroyGUI(mpDevice);
	}

	void update(float delta)
	{
		mpSwapchain->waitForFence();

		if (!keyboardCapturedByGUI() && !mouseCapturedByGUI()) {
			mpCamera->update(delta, getCursorDelta());
		}

		mTime += delta;

		mLightPosition = glm::vec3(
			7.0 * sin(mTime / 3.0),
			2.5 * sin(mTime / 5.0) + 2.5,
			0.0
		);
	}

	void render() override
	{
		// Check if camera matrix and attachments need to be updated
		if (mpSwapchain->recreated()) {
			mpCamera->updateAspectRatio();
			createAttachments();
			createAttachmentDescriptor();
			createRayTracingDescriptor();
			mpGBufferPass->update(mColorAttachments, mpDepthAttachment);
			mpResolvePass->update(mpSwapchain->getExtent());
		}

		// Acquire frame from swapchain and prepare rasterizer
		VkCommandBuffer cmd = mpSwapchain->acquireNextImage();

		// Transition color attachments to correct image layout
		transitionAttachmentsForGBuffer(cmd);

		// Begin G-Buffer pass
		mpGBufferPass->begin(cmd, glm::vec4(0.2f, 0.6f, 1.0f, 1.0f));

		// Render scene
		mpScene->render(cmd, mpCamera, mpSwapchain->getInFlightIndex());

		// End the G-Buffer pass without any implicit image transitions
		vkCmdEndRendering(cmd);

		// Transition color attachments for ray tracing
		transitionAttachmentsForRayTracing(cmd);

		// Bind ray tracing pipeline
		RayTracingPipeline* pRtPipeline = dynamic_cast<RayTracingPipeline*>(mPipelines[RT_PASS].get());
		pRtPipeline->bind(cmd);

		// Push constants
		PushConstants pushConstants = {
			.lightPosition = mLightPosition,
			.aoDistance = mAoDistance,
			.renderMode = mRenderMode,
		};
		vkCmdPushConstants(cmd, pRtPipeline->getLayout(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0,
			sizeof(PushConstants), &pushConstants);

		// Prepare color attachments for ray tracing
		Helpers::imageBarrier(cmd, mColorAttachments[3]->getImage(), VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
			VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

		mpScene->bindRayTracingDescriptors(cmd, mpCamera, pRtPipeline->getLayout(), mpSwapchain->getInFlightIndex());
		mpRayTracingDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pRtPipeline->getLayout(), 2);

		// Trace rays
		auto rayGenSBT = pRtPipeline->getRayGenSBT();
		auto missSBT = pRtPipeline->getMissSBT();
		auto hitSBT = pRtPipeline->getHitSBT();
		auto callSBT = pRtPipeline->getCallSBT();
		vkCmdTraceRaysKHR(cmd, &rayGenSBT, &missSBT, &hitSBT, &callSBT, mpSwapchain->getExtent().width,
			mpSwapchain->getExtent().height, 1);

		// Transition color attachments for resolve
		transitionAttachmentsForResolve(cmd);

		// Resolve
		mpResolvePass->begin(cmd);

		// Bind the pipeline for the full-screen quad
		mPipelines[RESOLVE_PASS]->bind(cmd);

		// Bind descriptors for color attachments
		mpColorAttachmentDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[RESOLVE_PASS]->getLayout(),
			0);

		// Push constants
		vkCmdPushConstants(cmd, mPipelines[RESOLVE_PASS]->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			sizeof(PushConstants), &pushConstants);

		// Render full-screen quad to resolve final composition
		vkCmdDraw(cmd, 3, 1, 0, 0);

		// Draw GUI
		App::renderGUI(cmd);

		// Submit command buffer to rasterizer and present swapchain frame
		mpResolvePass->end(cmd);
		mpSwapchain->present(cmd, mpResolvePass->getOutput());
	}

	void appGUI(ImGuiContext* pContext)
	{
		ImGui::SetCurrentContext(pContext);

		// Render the base GUI, the menu bar with it's subwindows
		App::baseGUI(mpDevice, mpSwapchain, mPipelines);

		if (ImGui::Begin("DSRT")) {
			const char* renderModes[] = {
				"Resolved",
				"Position",
				"Normal",
				"Albedo",
				"Shadow",
				"Ambient Occlusion",
			};
			ImGui::Combo("Render mode", &mRenderMode, renderModes, IM_ARRAYSIZE(renderModes));
			ImGui::SliderFloat("AO Distance", &mAoDistance, 0.1f, 20.0f);
		}

		ImGui::End();
	}

	void appKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		App::baseKeyCallback(window, key, scancode, action, mods, mpDevice, mpSwapchain, mPipelines);
	}

	void appCursorPosCallback(GLFWwindow* pWindow, double xPos, double yPos)
	{
		App::baseCursorPosCallback(pWindow, xPos, yPos);
	}

	void appMouseButtonCallback(GLFWwindow* pWindow, int button, int action, int mods)
	{
		App::baseMouseButtonCallback(pWindow, button, action, mods, mpCamera);
	}


private:
	std::shared_ptr<Device> mpDevice;
	std::shared_ptr<Swapchain> mpSwapchain;
	std::shared_ptr<Pass> mpGBufferPass;
	std::shared_ptr<Pass> mpResolvePass;
	std::vector<std::shared_ptr<Pipeline>> mPipelines;

	std::vector<std::shared_ptr<Image>> mColorAttachments;
	std::shared_ptr<Descriptor> mpColorAttachmentDescriptor;

	std::shared_ptr<Image> mpDepthAttachment;

	std::shared_ptr<Descriptor> mpRayTracingDescriptor;

	std::shared_ptr<AccelerationStructure> mpAccelerationStructure;
	std::shared_ptr<Scene> mpScene;
	std::shared_ptr<Camera> mpCamera;

	std::vector<uint32_t> mSpecializationConstants;
	std::vector<VkSpecializationMapEntry> mSpecializationMapEntries;
	VkSpecializationInfo mSpecializationInfo;

	float mTime = 0.0f;
	glm::vec3 mLightPosition = glm::vec3(0.0f, 0.0f, 0.0f);
	float mAoDistance = 5.0f;

	int mRenderMode = 0;
};

int main()
{
	DSRT app = DSRT();
	app.run();
	return 0;
}
