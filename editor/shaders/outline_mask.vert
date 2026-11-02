#version 450

#include <yqlora/uniforms.glsl>
#include <yqlora/vertex.glsl>

layout(location = YQLORA_POSITION_LOCATION) in vec3 aPosition;

void main()
{
    gl_Position = view.viewProjection * object.model * vec4(aPosition, 1.0);
}
