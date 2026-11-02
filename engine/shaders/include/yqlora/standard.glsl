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

#endif

#endif
