#version 450

layout(set = 2, binding = 0, std140) uniform Background {
    // В sRGB: итоговая цель хранит закодированный цвет.
    vec4 color;
} background;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = background.color;
}
