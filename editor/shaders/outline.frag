#version 450

layout(location = 0) in vec2 vUv;

layout(set = 2, binding = 0, std140) uniform Outline {
    vec4 color;
} outline;

layout(set = 2, binding = 1) uniform sampler2D mask;

layout(location = 0) out vec4 fragColor;

// Толщина контура в пикселях: ищем маску в круге такого радиуса.
const int kRadius = 2;
// Маска - 0 или 1, всё больше половины считаем объектом.
const float kInside = 0.5;

void main()
{
    // Внутри объекта контур не рисуем.
    if (texture(mask, vUv).r > kInside) {
        discard;
    }

    // Пиксель снаружи, но маска рядом: это контур.
    vec2 texel = 1.0 / vec2(textureSize(mask, 0));
    float nearMask = 0.0;
    for (int x = -kRadius; x <= kRadius; ++x) {
        for (int y = -kRadius; y <= kRadius; ++y) {
            // Чуть больше радиуса в квадрате: круг без зазубрин по диагоналям.
            if (x * x + y * y <= kRadius * kRadius + 1) {
                nearMask = max(nearMask, texture(mask, vUv + vec2(x, y) * texel).r);
            }
        }
    }
    if (nearMask < kInside) {
        discard;
    }
    fragColor = outline.color;
}
