#version 450

#include <yqlora/uniforms.glsl>
#include "grid.glsl"

layout(location = 0) in vec2 aCorner;

layout(location = 0) out vec3 vWorld;

void main()
{
    vWorld = vec3(view.cameraPosition.x + aCorner.x * grid.fadeDistance, 0.0,
                  view.cameraPosition.z + aCorner.y * grid.fadeDistance);
    gl_Position = view.viewProjection * vec4(vWorld, 1.0);
}
