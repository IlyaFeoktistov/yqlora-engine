// Блоки движка по частоте смены: кадр, вид, объект. Раскладка std140
// повторяет структуры C++ (engine/internal/yqlora/render/ShaderData.hpp) и
// сверяется с ними при загрузке шейдера: имена полей те же.
#ifndef YQLORA_UNIFORMS_GLSL
#define YQLORA_UNIFORMS_GLSL

layout(set = 0, binding = 0, std140) uniform Frame {
    vec3 lightDirection;
} frame;

layout(set = 1, binding = 0, std140) uniform View {
    mat4 viewProjection;
    vec3 cameraPosition;
} view;

layout(set = 3, binding = 0, std140) uniform Object {
    mat4 model;
    // mat3 в std140 - три vec4, проще передавать mat4.
    mat4 normalMatrix;
    vec4 color;
} object;

#endif
