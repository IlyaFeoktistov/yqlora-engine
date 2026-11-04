#version 450

// Таблица BRDF для отражений неба (split-sum, Karis 2013): по косинусу
// взгляда (u) и шероховатости (v) - множитель A и добавка B к F0, так что
// доля отражённого света - F0 * A + B. Не зависит ни от неба, ни от сцены:
// строится один раз.
#include <yqlora/brdf.glsl>

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

const uint kSamples = 256u;

void main()
{
    float cosNormalView = max(vUv.x, YQLORA_MIN_COSINE);
    float roughness = max(vUv.y, YQLORA_MIN_ROUGHNESS);
    float alpha = roughness * roughness;
    vec3 toView = vec3(sqrt(1.0 - cosNormalView * cosNormalView), 0.0, cosNormalView);
    float scale = 0.0;
    float bias = 0.0;
    for (uint i = 0u; i < kSamples; ++i) {
        vec3 halfway = importanceSampleGgx(hammersley(i, kSamples), alpha);
        vec3 light = reflect(-toView, halfway);
        float cosNormalLight = light.z;
        if (cosNormalLight <= 0.0) {
            continue;
        }
        float cosNormalHalf = max(halfway.z, YQLORA_MIN_COSINE);
        float cosViewHalf = max(dot(toView, halfway), 0.0);
        // BRDF на косинус, делённая на плотность выборки GGX.
        float shadowing = visibilitySmith(cosNormalView, cosNormalLight, alpha) * 4.0 * cosViewHalf
                          * cosNormalLight / cosNormalHalf;
        float fresnel = pow(1.0 - cosViewHalf, 5.0);
        scale += (1.0 - fresnel) * shadowing;
        bias += fresnel * shadowing;
    }
    fragColor = vec4(scale / float(kSamples), bias / float(kSamples), 0.0, 1.0);
}
