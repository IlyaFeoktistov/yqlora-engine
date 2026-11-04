#version 450

// Таблица многократного рассеяния (Hillaire, 2020, раздел 5.5.2): по высоте
// и углу солнца к зениту. Свет второго порядка, пришедший со всех сторон,
// и доля, которую воздух переизлучает, дают сумму всех порядков как
// геометрический ряд: L2 / (1 - f).
#include "march.glsl"

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

// Направления на сфере: равные телесные углы.
const int kRows = 8;
const int kColumns = 8;
const int kSteps = 20;
const float kNoLimit = 1e9;

void main()
{
    float cosSun = vUv.x * 2.0 - 1.0;
    float radius = mix(atmosphere.bottomRadius + YQLORA_ATMOSPHERE_MIN_HEIGHT, atmosphere.topRadius, vUv.y);
    vec3 origin = vec3(0.0, radius, 0.0);
    vec3 toSun = vec3(sqrt(max(1.0 - cosSun * cosSun, 0.0)), cosSun, 0.0);

    vec3 secondOrder = vec3(0.0);
    vec3 transfer = vec3(0.0);
    for (int row = 0; row < kRows; ++row) {
        float y = 1.0 - 2.0 * (float(row) + 0.5) / float(kRows);
        float ring = sqrt(max(1.0 - y * y, 0.0));
        for (int column = 0; column < kColumns; ++column) {
            float angle = 2.0 * YQLORA_ATMOSPHERE_PI * (float(column) + 0.5) / float(kColumns);
            vec3 direction = vec3(ring * cos(angle), y, ring * sin(angle));
            AtmosphereMarch march = marchAtmosphere(origin, direction, toSun, kNoLimit, kSteps, true, true);
            secondOrder += march.luminance;
            transfer += march.transfer;
        }
    }
    float count = float(kRows * kColumns);
    secondOrder /= count;
    transfer /= count;
    fragColor = vec4(secondOrder / max(vec3(1.0) - transfer, vec3(YQLORA_ATMOSPHERE_EPSILON)), 1.0);
}
