#version 330 core

in vec2 vUv;

uniform sampler2D uMask;

layout(std140) uniform Outline {
    vec4 uColor;
};

out vec4 fragColor;

// Толщина контура в пикселях: ищем маску в круге такого радиуса.
const int kRadius = 2;
// Маска - 0 или 1, всё больше половины считаем объектом.
const float kInside = 0.5;

void main()
{
    // Внутри объекта контур не рисуем.
    if (texture(uMask, vUv).r > kInside) {
        discard;
    }

    // Пиксель снаружи, но маска рядом: это контур.
    vec2 texel = 1.0 / vec2(textureSize(uMask, 0));
    float nearMask = 0.0;
    for (int x = -kRadius; x <= kRadius; ++x) {
        for (int y = -kRadius; y <= kRadius; ++y) {
            // Чуть больше радиуса в квадрате: круг без зазубрин по диагоналям.
            if (x * x + y * y <= kRadius * kRadius + 1) {
                nearMask = max(nearMask, texture(uMask, vUv + vec2(x, y) * texel).r);
            }
        }
    }
    if (nearMask < kInside) {
        discard;
    }
    fragColor = uColor;
}
