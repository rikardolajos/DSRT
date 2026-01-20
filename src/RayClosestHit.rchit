#version 460
#extension GL_EXT_ray_tracing : enable

#include "RayPayload.glsl"

// Specialization constant should be generated from scene information
layout (constant_id = 0) const uint VERTEX_COUNT = 1;
layout (constant_id = 1) const uint INDEX_COUNT = 1;
layout (constant_id = 2) const uint MATERIAL_COUNT = 1;
layout (constant_id = 3) const uint TEXTURE_COUNT = 1;
layout (constant_id = 4) const uint MESH_COUNT = 1;

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 texcoord;
    vec3 tangent;
    vec3 binormal;
    float _padding; // To enforce same size and alignment as host
};

layout(set = 1, binding = 1, std430) readonly buffer VertexBuffer {
	Vertex vertices[VERTEX_COUNT];
};

layout(set = 1, binding = 2, std430) readonly buffer IndexBuffer {
	uint indices[INDEX_COUNT];
};

struct InstanceData {
    uint verticesOffset;
    uint indicesOffset;
};

layout(set = 1, binding = 3, std430) readonly buffer InstanceDataBuffer {
	InstanceData instanceDatas[MESH_COUNT];
};

const uint DIFFUSE_TEXTURE_BIT = 1 << 0;
const uint SPECULAR_TEXTURE_BIT = 1 << 1;
const uint AMBIENT_TEXTURE_BIT = 1 << 2;
const uint EMISSION_TEXTURE_BIT = 1 << 3;
const uint NORMAL_TEXTURE_BIT = 1 << 4;

struct MaterialParams {
    vec3 diffuse;
    float shininess;
    vec3 specular;
    float indexOfRefraction;
    vec3 ambient;
    float opacity;
    vec3 emission;
    uint hasTexture;
};

struct Material {
    MaterialParams params;
    uint diffuseTextureIndex;
    uint specularTextureIndex;
    uint ambientTextureIndex;
    uint emissionTextureIndex;
    uint normalTextureIndex;
    uint _padding0;  // To enforce same size and alignment as host
    uint _padding1;
    uint _padding2;
};

layout(set = 1, binding = 4, std430) readonly buffer MaterialBuffer {
	Material materials[MATERIAL_COUNT];
};

layout(set = 1, binding = 5) uniform sampler2D textures[TEXTURE_COUNT];

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec3 attribs;

void main()
{
    rayPayload.hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
}
