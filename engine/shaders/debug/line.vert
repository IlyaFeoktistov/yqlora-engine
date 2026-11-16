#version 450

#include <yqlora/uniforms.glsl>

layout(location = 0) in vec3 aPosition;
// Линейный цвет: без экспозиции вида, отладка видна такой, как её задали.
layout(location = 1) in vec4 aColor;

layout(location = 0) out vec4 vColor;

void main()
{
    vColor = aColor;
    gl_Position = view.viewProjection * vec4(aPosition, 1.0);
}
