// Блоки движка по частоте смены: кадр, вид, объект. Раскладка std140
// повторяет структуры C++ (engine/internal/yqlora/render/EngineUniforms.hpp) и
// сверяется с ними при загрузке шейдера: имена полей и размеры массивов те же.
#ifndef YQLORA_UNIFORMS_GLSL
#define YQLORA_UNIFORMS_GLSL

#define YQLORA_MAX_DIRECTIONAL_LIGHTS 4
#define YQLORA_MAX_PUNCTUAL_LIGHTS 32

struct DirectionalLight {
    // Куда свет идёт, единичный.
    vec3 direction;
    // Освещённость в люксах по каналам.
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
    // Куда идёт свет солнца, единичный; без солнца hasSun = 0. Солнце -
    // directionalLights[0].
    vec3 sunDirection;
    int hasSun;
    // Освещённость от солнца в люксах по каналам, до атмосферы.
    vec3 sunColor;
    int directionalCount;
    int punctualCount;
    // Множитель света и отражений неба (Environment::ambientIntensity).
    float ambientIntensity;
    // Последний мип куба отражений: мип - шероховатость, умноженная на него.
    float environmentMaxMip;
    // Угловой радиус солнца в радианах: диск на небе.
    float sunAngularRadius;
    DirectionalLight directionalLights[YQLORA_MAX_DIRECTIONAL_LIGHTS];
    PunctualLight punctualLights[YQLORA_MAX_PUNCTUAL_LIGHTS];
} frame;

layout(set = 1, binding = 0, std140) uniform View {
    mat4 viewProjection;
    // Из точки экрана в мир: по ней небо находит направление пикселя.
    mat4 inverseViewProjection;
    vec3 cameraPosition;
    // Множитель экспозиции: всё, что пишется в HDR-цель, умножается на него.
    float exposure;
} view;

layout(set = 3, binding = 0, std140) uniform Object {
    mat4 model;
    // mat3 в std140 - три vec4, проще передавать mat4.
    mat4 normalMatrix;
    // 0 - тени на объект не падают (MeshRenderer::receiveShadows).
    int receiveShadows;
} object;

#endif
