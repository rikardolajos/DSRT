#version 460

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBinormal;
layout(location = 5) in mat3 inNormalMatrix;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

layout(set = 2, binding = 0) uniform MaterialParams {
    vec3 diffuse;
    float shininess;
    vec3 specular;
    float indexOfRefraction;
    vec3 ambient;
    float opacity;
    vec3 emission;
    uint hasTexture;
} materialParams;

layout(set = 2, binding = 1) uniform sampler2D diffuseTexture;
layout(set = 2, binding = 2) uniform sampler2D specularTexture;
layout(set = 2, binding = 3) uniform sampler2D ambientTexture;
layout(set = 2, binding = 4) uniform sampler2D emissionTexture;
layout(set = 2, binding = 5) uniform sampler2D normalTexture;

const uint DIFFUSE_TEXTURE_BIT  = 1 << 0;
const uint SPECULAR_TEXTURE_BIT = 1 << 1;
const uint AMBIENT_TEXTURE_BIT = 1 << 2;
const uint EMISSION_TEXTURE_BIT = 1 << 3;
const uint NORMAL_TEXTURE_BIT = 1 << 4;

void main() {
    // Discard transparent parts
    if ((materialParams.hasTexture & DIFFUSE_TEXTURE_BIT) != 0) {
        if (texture(diffuseTexture, inTexCoord).a < 0.5) {
            discard;
        }
    }

    outPosition = vec4(inWorldPos, 1.0);

    if ((materialParams.hasTexture & NORMAL_TEXTURE_BIT) != 0) {
        mat3 TBN = mat3(normalize(inTangent), normalize(inBinormal), normalize(inNormal));
        vec3 normal = texture(normalTexture, inTexCoord).rgb * 2.0 - 1.0;
        normal.y *= -1.0; // Normal map for Sponza is in DirectX convention, flip it
        outNormal.xyz = normalize(inNormalMatrix * TBN * normal);
    } else {
        outNormal = vec4(inNormalMatrix * inNormal, 1.0);
    }

    if ((materialParams.hasTexture & DIFFUSE_TEXTURE_BIT) != 0) {
        outAlbedo = texture(diffuseTexture, inTexCoord);
    } else {
        outAlbedo = vec4(materialParams.diffuse, 1.0);
    }
}
