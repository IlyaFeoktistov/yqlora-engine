// Блоки движка по частоте смены: кадр, вид, объект. Раскладка std140
// повторяет структуры C++ (engine/internal/yqlora/render/ShaderData.hpp) и
// сверяется с ними при загрузке шейдера: имена полей и размеры массивов те же.
#ifndef YQLORA_UNIFORMS_GLSL
#define YQLORA_UNIFORMS_GLSL

#define YQLORA_MAX_DIRECTIONAL_LIGHTS 4
#define YQLORA_MAX_PUNCTUAL_LIGHTS 32

struct DirectionalLight {
    // Куда свет идёт, единичный.
    vec3 direction;
    // Линейный цвет, умноженный на интенсивность.
    vec3 color;
};

// Точечный свет и прожектор. Конус - saturate(cos * angleScale + angleOffset),
// у точечного это всегда 1.
struct PunctualLight {
    vec3 position;
    float range;
    vec3 color;
    float angleScale;
    vec3 direction;
    float angleOffset;
};

layout(set = 0, binding = 0, std140) uniform Frame {
    // Окружающий свет: линейный цвет, умноженный на интенсивность.
    vec3 ambient;
    int directionalCount;
    int punctualCount;
    DirectionalLight directionalLights[YQLORA_MAX_DIRECTIONAL_LIGHTS];
    PunctualLight punctualLights[YQLORA_MAX_PUNCTUAL_LIGHTS];
} frame;

layout(set = 1, binding = 0, std140) uniform View {
    mat4 viewProjection;
    vec3 cameraPosition;
} view;

layout(set = 3, binding = 0, std140) uniform Object {
    mat4 model;
    // mat3 в std140 - три vec4, проще передавать mat4.
    mat4 normalMatrix;
} object;

#endif
