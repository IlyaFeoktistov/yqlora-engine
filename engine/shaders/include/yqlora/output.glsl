// Проход вывода: линейный HDR-цвет кадра в sRGB итоговой цели. Позже здесь же
// тональная компрессия и экспозиция; пока всё выше 1 обрезается.
#ifndef YQLORA_OUTPUT_GLSL
#define YQLORA_OUTPUT_GLSL

// Точная кривая sRGB (IEC 61966-2-1), обратная к srgbToLinear в C++.
vec3 linearToSrgb(vec3 color)
{
    const float kLinearSegmentEnd = 0.0031308;
    const float kLinearSlope = 12.92;
    const float kOffset = 0.055;
    const float kGamma = 2.4;
    color = clamp(color, 0.0, 1.0);
    vec3 curve = (1.0 + kOffset) * pow(color, vec3(1.0 / kGamma)) - kOffset;

    return mix(color * kLinearSlope, curve, step(kLinearSegmentEnd, color));
}

vec4 encodeOutput(vec4 linearColor)
{
    return vec4(linearToSrgb(linearColor.rgb), clamp(linearColor.a, 0.0, 1.0));
}

#endif
