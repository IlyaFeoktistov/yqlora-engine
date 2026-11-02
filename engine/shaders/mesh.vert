#version 450

#include <yqlora/uniforms.glsl>
#include <yqlora/vertex.glsl>

layout(location = YQLORA_POSITION_LOCATION) in vec3 aPosition;
layout(location = YQLORA_NORMAL_LOCATION) in vec3 aNormal;

layout(location = 0) out vec3 vNormal;

void main()
{
    vNormal = mat3(object.normalMatrix) * aNormal;
    gl_Position = view.viewProjection * object.model * vec4(aPosition, 1.0);
}
