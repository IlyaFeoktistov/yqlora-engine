// Всё для обычного освещённого шейдера в одном файле (.yshader): блоки
// движка, освещение, входы вершин и переменные между стадиями. Стадию
// выбирает определение YQLORA_VERTEX или YQLORA_FRAGMENT, его ставит движок.
#ifndef YQLORA_STANDARD_GLSL
#define YQLORA_STANDARD_GLSL

#include <yqlora/lighting.glsl>
#include <yqlora/output.glsl>
#include <yqlora/uniforms.glsl>
#include <yqlora/vertex.glsl>

#ifdef YQLORA_VERTEX

layout(location = YQLORA_POSITION_LOCATION) in vec3 aPosition;
layout(location = YQLORA_NORMAL_LOCATION) in vec3 aNormal;
layout(location = YQLORA_UV0_LOCATION) in vec2 aUv0;
layout(location = YQLORA_TANGENT_LOCATION) in vec4 aTangent;

layout(location = 0) out vec3 vWorldPosition;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUv0;
// Касательная в мире и знак бинормали: bitangent = cross(normal, xyz) * w.
layout(location = 3) out vec4 vTangent;

// Вершина меша в мир и на экран.
void standardVertex()
{
    vec4 world = object.model * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    vNormal = mat3(object.normalMatrix) * aNormal;
    vUv0 = aUv0;
    vTangent = vec4(mat3(object.model) * aTangent.xyz, aTangent.w);
    gl_Position = view.viewProjection * world;
}

#endif

#ifdef YQLORA_FRAGMENT

layout(location = 0) in vec3 vWorldPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUv0;
layout(location = 3) in vec4 vTangent;

layout(location = 0) out vec4 fragColor;

// Поверхность в точке фрагмента; остальное - как в defaultSurface(position, normal).
Surface defaultSurface()
{
    return defaultSurface(vWorldPosition, normalize(vNormal));
}

// Короче этого касательная считается вырожденной (меш без касательных).
const float YQLORA_MIN_TANGENT_LENGTH = 0.0001;

// Касательное пространство фрагмента в мире: столбцы - касательная,
// бинормаль, нормаль. У меша без касательных - любой базис вокруг нормали.
mat3 tangentFrame()
{
    vec3 normal = normalize(vNormal);
    vec3 tangent = vTangent.xyz - normal * dot(normal, vTangent.xyz);
    if (dot(tangent, tangent) < YQLORA_MIN_TANGENT_LENGTH) {
        tangent = abs(normal.x) < 0.9 ? cross(normal, vec3(1.0, 0.0, 0.0)) : cross(normal, vec3(0.0, 1.0, 0.0));
    }
    tangent = normalize(tangent);
    float handedness = vTangent.w < 0.0 ? -1.0 : 1.0;

    return mat3(tangent, cross(normal, tangent) * handedness, normal);
}

// Нормаль в мире из значения карты нормалей (касательное пространство,
// +Y вверх, как в glTF); scale усиливает наклон, 0 - нормаль меша.
vec3 normalFromMap(vec3 sampled, float scale)
{
    vec3 normal = sampled * 2.0 - 1.0;
    normal.xy *= scale;

    return normalize(tangentFrame() * normal);
}

// Шаги параллакса: вдоль поверхности их нужно больше, чем при взгляде сверху.
const float YQLORA_PARALLAX_MIN_STEPS = 8.0;
const float YQLORA_PARALLAX_MAX_STEPS = 32.0;

// Parallax occlusion mapping: текстурные координаты точки, которую видно
// сквозь рельеф карты высот (1 - верх, 0 - глубина depth в единицах UV).
// depth 0 - параллакса нет, и ничего не считается.
vec2 parallaxUv(sampler2D heightMap, vec2 uv, float depth)
{
    if (depth <= 0.0) {
        return uv;
    }
    vec3 toView = normalize(transpose(tangentFrame()) * (view.cameraPosition - vWorldPosition));
    float steps = mix(YQLORA_PARALLAX_MAX_STEPS, YQLORA_PARALLAX_MIN_STEPS, abs(toView.z));
    float layer = 1.0 / steps;
    vec2 shift = toView.xy / max(toView.z, YQLORA_MIN_TANGENT_LENGTH) * depth / steps;
    float rayDepth = 0.0;
    float surfaceDepth = 1.0 - texture(heightMap, uv).r;
    vec2 previousUv = uv;
    float previousGap = surfaceDepth;
    for (float i = 0.0; i < YQLORA_PARALLAX_MAX_STEPS && rayDepth < surfaceDepth; i += 1.0) {
        previousUv = uv;
        previousGap = surfaceDepth - rayDepth;
        uv -= shift;
        rayDepth += layer;
        surfaceDepth = 1.0 - texture(heightMap, uv).r;
    }
    // Между двумя последними шагами - по линейной интерполяции.
    float gap = surfaceDepth - rayDepth;
    float weight = gap / (gap - previousGap);

    return mix(uv, previousUv, clamp(weight, 0.0, 1.0));
}

#endif

#endif
