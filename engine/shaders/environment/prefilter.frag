#version 450

// Мип куба отражений: небо, размытое по GGX для шероховатости мипа
// (filtered importance sampling, Karis 2013 и Colbert 2007): выборки по
// распределению блика, каждая с мипа источника по своему телесному углу,
// чтобы не было пятен от редких ярких точек.
#include <yqlora/brdf.glsl>
#include <yqlora/cube.glsl>

layout(set = 2, binding = 0) uniform samplerCube source;

// Повторяет PrefilterUniforms в EnvironmentLighting.cpp.
layout(set = 2, binding = 1, std140) uniform Prefilter {
    int face;
    // Сторона грани этого мипа в пикселях.
    float size;
    float roughness;
} prefilter;

layout(location = 0) out vec4 fragColor;

const uint kSamples = 64u;

void main()
{
    vec3 normal = cubeDirection(prefilter.face, gl_FragCoord.xy / prefilter.size);
    if (prefilter.roughness <= 0.0) {
        fragColor = vec4(textureLod(source, normal, 0.0).rgb, 1.0);
        return;
    }
    float alpha = prefilter.roughness * prefilter.roughness;
    mat3 basis = tangentBasis(normal);
    float sourceSize = float(textureSize(source, 0).x);
    float texelSolidAngle = 4.0 * YQLORA_PI / (6.0 * sourceSize * sourceSize);
    vec3 sum = vec3(0.0);
    float weight = 0.0;
    for (uint i = 0u; i < kSamples; ++i) {
        vec3 halfway = basis * importanceSampleGgx(hammersley(i, kSamples), alpha);
        // Взгляд вдоль нормали: так все мипы считаются одной таблицей.
        vec3 light = reflect(-normal, halfway);
        float cosNormalLight = dot(normal, light);
        if (cosNormalLight <= 0.0) {
            continue;
        }
        float cosNormalHalf = max(dot(normal, halfway), 0.0);
        float pdf = distributionGgx(cosNormalHalf, alpha) * 0.25;
        float sampleSolidAngle = 1.0 / (float(kSamples) * pdf + YQLORA_MIN_COSINE);
        // Сдвиг на мип вверх: чуть больше размытия, меньше пятен.
        float lod = max(0.5 * log2(sampleSolidAngle / texelSolidAngle) + 1.0, 0.0);
        sum += textureLod(source, light, lod).rgb * cosNormalLight;
        weight += cosNormalLight;
    }
    fragColor = vec4(sum / max(weight, YQLORA_MIN_COSINE), 1.0);
}
