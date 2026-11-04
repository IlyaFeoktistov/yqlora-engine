// Всё для шейдера неба (.yshader): стадии вокруг одной функции, которую
// пишет сам шейдер,
//     vec3 sky(vec3 direction)
// - яркость неба в нитах в направлении direction от камеры (оси мира,
// единичное). Шейдер, подключивший этот файл, движок считает шейдером неба:
// его материал годится только в небо окружения.
//
// sky() вызывается только во фрагментной стадии; если она пользуется тем,
// чего нет в вершинной (dFdx, dFdy), её определение обёртывается в
// #ifdef YQLORA_FRAGMENT.
//
// Треугольник на весь экран на дальней плоскости, каждый пиксель - sky()
// вдоль своего луча. Движок собирает шейдер неба ещё и с определением
// YQLORA_SKY_CAPTURE: так небо рисуется в куб для освещения сцены, в нитах
// без экспозиции. В этом варианте шейдер пропускает то, что уже дают
// источники света, например диск солнца.
#ifndef YQLORA_SKY_GLSL
#define YQLORA_SKY_GLSL

#include <yqlora/exposure.glsl>
#include <yqlora/uniforms.glsl>

vec3 sky(vec3 direction);

#ifdef YQLORA_VERTEX

layout(location = 0) out vec2 vClip;

void main()
{
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vClip = uv * 2.0 - 1.0;
    // z = w: глубина 1, небо за всем, что нарисовано.
    gl_Position = vec4(vClip, 1.0, 1.0);
}

#endif

#ifdef YQLORA_FRAGMENT

layout(location = 0) in vec2 vClip;
layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 far = view.inverseViewProjection * vec4(vClip, 1.0, 1.0);
    vec3 direction = normalize(far.xyz / far.w - view.cameraPosition);
#ifdef YQLORA_SKY_CAPTURE
    fragColor = vec4(min(sky(direction), vec3(YQLORA_MAX_EXPOSED)), 1.0);
#else
    fragColor = vec4(exposed(sky(direction)), 1.0);
#endif
}

#endif

#endif
