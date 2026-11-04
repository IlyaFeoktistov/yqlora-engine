#version 450

// Таблица пропускания: по высоте и углу к зениту, до верха атмосферы.
#include <yqlora/atmosphere.glsl>

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

const int kSteps = 40;

void main()
{
    float radius = 0.0;
    float cosZenith = 0.0;
    transmittanceParameters(vUv, radius, cosZenith);
    vec3 origin = vec3(0.0, radius, 0.0);
    vec3 direction = vec3(sqrt(max(1.0 - cosZenith * cosZenith, 0.0)), cosZenith, 0.0);
    float rayLength = max(raySphere(origin, direction, atmosphere.topRadius), 0.0);
    float stepLength = rayLength / float(kSteps);
    vec3 depth = vec3(0.0);
    for (int i = 0; i < kSteps; ++i) {
        vec3 position = origin + direction * (float(i) + 0.5) * stepLength;
        depth += atmosphereMedium(length(position) - atmosphere.bottomRadius).extinction * stepLength;
    }
    fragColor = vec4(exp(-depth), 1.0);
}
