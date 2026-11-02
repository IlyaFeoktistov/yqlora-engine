#version 450

#include <yqlora/output.glsl>

// HDR-цель кадра того же размера, что итоговая: пиксель в пиксель.
layout(set = 2, binding = 0) uniform sampler2D hdrColor;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = encodeOutput(texelFetch(hdrColor, ivec2(gl_FragCoord.xy), 0));
}
