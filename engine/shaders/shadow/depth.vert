#version 450

// Глубина меша в карту теней: блок View несёт матрицу карты (каскада или
// плитки), а не камеры.
#include <yqlora/uniforms.glsl>
#include <yqlora/vertex.glsl>

layout(location = YQLORA_POSITION_LOCATION) in vec3 position;

void main()
{
    gl_Position = view.viewProjection * object.model * vec4(position, 1.0);
}
