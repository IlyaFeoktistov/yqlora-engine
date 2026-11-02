#version 330 core

in vec3 vWorld;

layout(std140) uniform View {
    mat4 uViewProjection;
    vec3 uCameraPosition;
};

layout(std140) uniform Grid {
    float uCellSize;
    float uFadeDistance;
};

out vec4 fragColor;

// Каждая десятая линия - крупная, и видна дальше мелких.
const float kMajorEvery = 10.0;
// На какой доле uFadeDistance сетка начинает гаснуть; мелкие клетки - раньше.
const float kFadeStart = 0.3;
const float kMinorFadeStart = 0.1;
const float kMinorFadeEnd = 0.4;
const vec3 kLineColor = vec3(0.5);
const float kMinorAlpha = 0.25;
const float kMajorAlpha = 0.5;
// Цвета осей как у гизмо: X красный, Z синий.
const vec4 kAxisXColor = vec4(0.9, 0.3, 0.3, 0.9);
const vec4 kAxisZColor = vec4(0.3, 0.45, 0.95, 0.9);
// Толщина линии оси в пикселях.
const float kAxisWidth = 1.5;
// Почти прозрачные пиксели не пишем, чтобы не портить глубину.
const float kMinAlpha = 0.002;

// 1 на линии сетки с шагом cell, 0 между линиями. Толщина линии около пикселя
// на любом расстоянии: fwidth говорит, сколько единиц мира приходится на пиксель.
float gridLines(vec2 coord, float cell)
{
    vec2 scaled = coord / cell;
    vec2 distanceToLine = abs(fract(scaled - 0.5) - 0.5) / fwidth(scaled);

    return 1.0 - min(min(distanceToLine.x, distanceToLine.y), 1.0);
}

float axisLine(float coord)
{
    return 1.0 - min(abs(coord) / (fwidth(coord) * kAxisWidth), 1.0);
}

void main()
{
    float distanceToCamera = length(vWorld - uCameraPosition);
    float fade = 1.0 - smoothstep(uFadeDistance * kFadeStart, uFadeDistance, distanceToCamera);
    // Мелкие клетки гаснут раньше: вдали они сливаются в рябь.
    float minorFade =
        1.0 - smoothstep(uFadeDistance * kMinorFadeStart, uFadeDistance * kMinorFadeEnd, distanceToCamera);

    float minor = gridLines(vWorld.xz, uCellSize) * minorFade;
    float major = gridLines(vWorld.xz, uCellSize * kMajorEvery);
    vec4 color = vec4(kLineColor, max(minor * kMinorAlpha, major * kMajorAlpha));

    // Ось X лежит на z = 0, ось Z на x = 0.
    color = mix(color, kAxisXColor, axisLine(vWorld.z));
    color = mix(color, kAxisZColor, axisLine(vWorld.x));

    color.a *= fade;
    if (color.a < kMinAlpha) {
        discard;
    }
    fragColor = color;
}
