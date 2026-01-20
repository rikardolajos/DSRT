#version 460

layout(set = 0, binding = 0) uniform CameraUniformDynamic {
    mat4 view;
    mat4 view_inv;
    mat4 proj;
    mat4 proj_inv;
} camera;

layout(set = 1, binding = 0) uniform MeshUniformDynamic {
    mat4 model;
} mesh;

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexTextureCoord;
layout(location = 3) in vec3 vertexTangent;
layout(location = 4) in vec3 vertexBinormal;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out vec3 outBinormal;
layout(location = 5) out mat3 outNormalMatrix;

void main() {
    outNormalMatrix = transpose(inverse(mat3(mesh.model)));

    outWorldPos = vec3(mesh.model * vec4(vertexPosition, 1.0));

    outTexCoord = vertexTextureCoord;

    outNormal = normalize(vertexNormal);
    outTangent = normalize(vertexTangent);
    outBinormal = normalize(vertexBinormal);

    gl_Position = camera.proj * camera.view * mesh.model * vec4(vertexPosition, 1.0);
}
