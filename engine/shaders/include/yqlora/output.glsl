// Проход вывода: линейная яркость кадра (уже с экспозицией) - тональная
// компрессия - sRGB итоговой цели. Компрессию выбирает определение
// YQLORA_TONE_MAPPING_<имя> (имена ToneMapping в C++), его ставит движок.
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

// AgX (Troy Sobotka): цвет сдвигается внутрь гаммы, сжимается в логарифме
// сигмоидой и возвращается. Матрицы и приближение сигмоиды многочленом -
// из минимальной реализации Benjamin Wrensch; вторая матрица обратна первой.
const mat3 YQLORA_AGX_INSET = mat3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772, 0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104
);
const mat3 YQLORA_AGX_OUTSET = mat3(
    1.19687900512017, -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.15107367264116
);
// Диапазон в ступенях экспозиции вокруг среднего серого 0.18.
const float YQLORA_AGX_MIN_EV = -12.47393;
const float YQLORA_AGX_MAX_EV = 4.026069;
// Показатель, которым кривая AgX переводится обратно в линейную яркость.
const float YQLORA_AGX_GAMMA = 2.2;

vec3 agxContrast(vec3 x)
{
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;

    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

vec3 toneMapAgx(vec3 color)
{
    vec3 encoded = log2(max(YQLORA_AGX_INSET * color, vec3(1e-10)));
    encoded = clamp((encoded - YQLORA_AGX_MIN_EV) / (YQLORA_AGX_MAX_EV - YQLORA_AGX_MIN_EV), 0.0, 1.0);
    vec3 display = YQLORA_AGX_OUTSET * agxContrast(encoded);

    return pow(max(display, vec3(0.0)), vec3(YQLORA_AGX_GAMMA));
}

// ACES: подгонка RRT и ODT Стивена Хилла. Матрицы по строкам, поэтому
// умножаются справа: v * M.
const mat3 YQLORA_ACES_INPUT = mat3(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777
);
const mat3 YQLORA_ACES_OUTPUT = mat3(
    1.60475, -0.53108, -0.07367,
    -0.10208, 1.10813, -0.00605,
    -0.00327, -0.07276, 1.07602
);

vec3 toneMapAces(vec3 color)
{
    vec3 v = color * YQLORA_ACES_INPUT;
    v = (v * (v + 0.0245786) - 0.000090537) / (v * (0.983729 * v + 0.4329510) + 0.238081);

    return v * YQLORA_ACES_OUTPUT;
}

// Khronos PBR Neutral: цвета ниже начала сжатия остаются как есть, яркие
// сжимаются к 1 и чуть теряют насыщенность.
vec3 toneMapNeutral(vec3 color)
{
    const float kStartCompression = 0.8 - 0.04;
    const float kDesaturation = 0.15;
    const float kToeEnd = 0.08;
    const float kToeScale = 6.25;
    const float kOffset = 0.04;
    float lowest = min(color.r, min(color.g, color.b));
    color -= lowest < kToeEnd ? lowest - kToeScale * lowest * lowest : kOffset;
    float peak = max(color.r, max(color.g, color.b));
    if (peak < kStartCompression) {
        return color;
    }
    const float kRange = 1.0 - kStartCompression;
    float newPeak = 1.0 - kRange * kRange / (peak + kRange - kStartCompression);
    color *= newPeak / peak;
    float desaturation = 1.0 - 1.0 / (kDesaturation * (peak - newPeak) + 1.0);

    return mix(color, vec3(newPeak), desaturation);
}

vec3 toneMap(vec3 color)
{
#if defined(YQLORA_TONE_MAPPING_AgX)
    return toneMapAgx(color);
#elif defined(YQLORA_TONE_MAPPING_Aces)
    return toneMapAces(color);
#elif defined(YQLORA_TONE_MAPPING_Neutral)
    return toneMapNeutral(color);
#else
    return color;
#endif
}

vec4 encodeOutput(vec4 linearColor)
{
    return vec4(linearToSrgb(toneMap(max(linearColor.rgb, vec3(0.0)))), clamp(linearColor.a, 0.0, 1.0));
}

#endif
