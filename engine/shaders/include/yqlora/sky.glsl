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
// YQLORA_SKY_CAPTURE: так небо рисуется в грань куба для освещения сцены
// (блок Capture: какая грань и её размер), в нитах без экспозиции. В этом
// варианте шейдер пропускает то, что уже дают источники света, например
// диск солнца.
#ifndef YQLORA_SKY_GLSL
#define YQLORA_SKY_GLSL

#include <yqlora/cube.glsl>
#include <yqlora/exposure.glsl>
#include <yqlora/uniforms.glsl>

vec3 sky(vec3 direction);

// Ступеней яркости (раз по 2), за которые край гаснет от белого на экране:
// меньше - спад идёт по всей ширине края, а не только у его начала.
const float YQLORA_DISC_EDGE_BLACK_STOPS = 4.0;
// Как быстро край тратит ступени выше белого: больше - раньше.
const float YQLORA_DISC_EDGE_BRIGHT_FALLOFF = 8.0;

// Мягкий край диска на небе (солнце): множитель его яркости в точке на
// fromCenter радиусов от центра. Сам диск (до 1) - в полную силу, за ним
// край гаснет на softness радиусов наружу, 0 - резкий край: мягкость не
// уменьшает видимый диск. exposedPeak - яркость диска после экспозиции, 1 -
// белый на экране. Диск солнца ярче белого в сотни раз: плавный спад самой
// яркости ушёл бы ниже белого только у внешней границы, и край выглядел бы
// резким. Поэтому ступени выше белого тратятся в начале края, а остальная
// его ширина гаснет от белого до чёрного по ступеням, как видит глаз, при
// любой экспозиции.
float softDiscEdge(float fromCenter, float softness, float exposedPeak)
{
    if (fromCenter <= 1.0) {
        return 1.0;
    }
    if (fromCenter >= 1.0 + softness) {
        return 0.0;
    }
    float t = smoothstep(0.0, 1.0, (fromCenter - 1.0) / softness);
    float brightStops = log2(max(exposedPeak, 1.0));
    float stops = brightStops * (1.0 - pow(1.0 - t, YQLORA_DISC_EDGE_BRIGHT_FALLOFF)) + YQLORA_DISC_EDGE_BLACK_STOPS * t;

    return exp2(-stops) * (1.0 - t);
}

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

#ifdef YQLORA_SKY_CAPTURE

// Повторяет CaptureUniforms в EngineUniforms.hpp.
layout(set = 3, binding = 1, std140) uniform Capture {
    int face;
    // Сторона грани в пикселях.
    float size;
} capture;

void main()
{
    vec3 direction = cubeDirection(capture.face, gl_FragCoord.xy / capture.size);
    fragColor = vec4(min(sky(direction), vec3(YQLORA_MAX_EXPOSED)), 1.0);
}

#else

void main()
{
    vec4 far = view.inverseViewProjection * vec4(vClip, 1.0, 1.0);
    vec3 direction = normalize(far.xyz / far.w - view.cameraPosition);
    fragColor = vec4(exposed(sky(direction)), 1.0);
}

#endif

#endif

#endif
