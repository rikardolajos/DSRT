#version 460

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0, rgba16f) uniform image2D inPosition;
layout(set = 0, binding = 1, rgba16f) uniform image2D inNormal;
layout(set = 0, binding = 2, rgba8) uniform image2D inAlbedo;
layout(set = 0, binding = 3, rgba8) uniform image2D inShadowDirectAndAO;

layout(push_constant) uniform PushConstant {
    vec3 lightPosition;
    float aoDistance;
    uint renderMode;
} pushConstant;

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    vec3 fragPos = imageLoad(inPosition, coord).rgb;
    vec3 normal = imageLoad(inNormal, coord).rgb;
    vec3 albedo = imageLoad(inAlbedo, coord).rgb;
    float shadow = imageLoad(inShadowDirectAndAO, coord).r;

    float ao = 0.0;
    const int kernelSize = 3;
    for (int x = -kernelSize; x < kernelSize; x++) {
        for (int y = -kernelSize; y < kernelSize; y++) {
            ao += imageLoad(inShadowDirectAndAO, coord + ivec2(x, y)).g;
        }
    }
    ao /= float(kernelSize * kernelSize);

    float d = length(pushConstant.lightPosition - fragPos);
    vec3 L = normalize(pushConstant.lightPosition - fragPos);
    vec3 N = normalize(normal);


    fragColor.a = 1.0;
    if (pushConstant.renderMode == 1) {
        fragColor.rgb = fragPos;
    } else if (pushConstant.renderMode == 2) {
        fragColor.rgb = N * 0.5 + 0.5;
    } else if (pushConstant.renderMode == 3) {
        fragColor.rgb = albedo;
    } else if (pushConstant.renderMode == 4) {
        fragColor.rgb = vec3(shadow);
    } else if (pushConstant.renderMode == 5) {
        fragColor.rgb = vec3(ao);
    } else{
        vec3 ambient = 0.15 * albedo * ao;
        vec3 diffuse = 5.0 * albedo * max(0.0, dot(N, L)) / (d * d) * shadow;
        fragColor = vec4(ambient + diffuse, 1.0);
    }
}
