#version 450

#include <yqlora/uniforms.glsl>

layout(location = 0) in vec3 vNormal;

layout(location = 0) out vec4 fragColor;

// Рассеянный свет плюс постоянная подсветка, чтобы теневые стороны не были чёрными.
const float kAmbient = 0.35;

void main()
{
    float diffuse = max(dot(normalize(vNormal), -frame.lightDirection), 0.0);
    float light = kAmbient + (1.0 - kAmbient) * diffuse;
    fragColor = vec4(object.color.rgb * light, object.color.a);
}
