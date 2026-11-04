#version 450

// Воздушная перспектива вдоль взгляда камеры вида: срезы по расстоянию
// (aerialSliceDistance) лежат в одну строку, каждый - квадрат экрана.
// Канал a - среднее пропускание. Число срезов задаёт движок
// (YQLORA_AERIAL_SLICES), его же видит читающий через textureSize.
#define YQLORA_ATMOSPHERE_MULTIPLE
#include "march.glsl"

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

const int kSteps = 16;

void main()
{
    float count = float(YQLORA_AERIAL_SLICES);
    float slice = min(floor(vUv.x * count), count - 1.0);
    vec2 screen = vec2(vUv.x * count - slice, vUv.y);
    vec4 far = view.inverseViewProjection * vec4(screen * 2.0 - 1.0, 1.0, 1.0);
    vec3 direction = normalize(far.xyz / far.w - view.cameraPosition);
    vec3 origin = atmospherePosition(view.cameraPosition);

    AtmosphereMarch march = marchAtmosphere(origin, direction, atmosphereToSun(), aerialSliceDistance(slice, count), kSteps, false, false);
    fragColor = vec4(march.luminance, dot(march.transmittance, vec3(1.0 / 3.0)));
}
