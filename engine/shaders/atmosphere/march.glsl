// Проход по лучу сквозь атмосферу: общий для таблиц многократного
// рассеяния, вида неба и воздушной перспективы. С определением
// YQLORA_ATMOSPHERE_MULTIPLE к однократному рассеянию добавляется
// многократное из своей таблицы (её нет только у прохода, который её пишет).
#ifndef YQLORA_ATMOSPHERE_MARCH_GLSL
#define YQLORA_ATMOSPHERE_MARCH_GLSL

#include <yqlora/atmosphere.glsl>

#ifdef YQLORA_ATMOSPHERE_MULTIPLE
layout(set = 2, binding = 0) uniform sampler2D multipleScatteringLut;

// Многократное рассеяние на единицу освещённости по высоте и солнцу.
vec3 multipleScattering(float radius, float cosSunZenith)
{
    vec2 uv = vec2(cosSunZenith * 0.5 + 0.5, (radius - atmosphere.bottomRadius) / (atmosphere.topRadius - atmosphere.bottomRadius));

    return texture(multipleScatteringLut, clamp(uv, 0.0, 1.0)).rgb;
}
#endif

// Отрезок внутри шага, где берётся среда: не край, а около середины.
const float YQLORA_MARCH_SAMPLE_OFFSET = 0.3;
const float YQLORA_UNIFORM_PHASE = 1.0 / (4.0 * YQLORA_ATMOSPHERE_PI);

struct AtmosphereMarch {
    // Рассеянный к началу луча свет на единицу освещённости от солнца.
    vec3 luminance;
    // Пропускание от начала луча до его конца.
    vec3 transmittance;
    // Доля света, которую воздух вдоль луча рассеивает к его началу от
    // равномерного источника единичной яркости (для многократного рассеяния).
    vec3 transfer;
};

// Луч из origin (координаты планеты, км) вдоль direction до земли, верха
// атмосферы или maxDistance. isotropic - рассеяние во все стороны поровну
// (так его считает таблица многократного рассеяния); ground - добавить
// землю, освещённую солнцем, если луч в неё упёрся.
AtmosphereMarch marchAtmosphere(vec3 origin, vec3 direction, vec3 toSun, float maxDistance, int steps, bool isotropic, bool ground)
{
    AtmosphereMarch result;
    result.luminance = vec3(0.0);
    result.transmittance = vec3(1.0);
    result.transfer = vec3(0.0);

    float toGround = raySphere(origin, direction, atmosphere.bottomRadius);
    float toTop = raySphere(origin, direction, atmosphere.topRadius);
    float rayLength = toGround >= 0.0 ? toGround : toTop;
    if (rayLength <= 0.0) {
        return result;
    }
    bool hitsGround = toGround >= 0.0 && toGround <= maxDistance;
    rayLength = min(rayLength, maxDistance);

    float cosAngle = dot(direction, toSun);
    float rayleigh = isotropic ? YQLORA_UNIFORM_PHASE : rayleighPhase(cosAngle);
    float mie = isotropic ? YQLORA_UNIFORM_PHASE : miePhase(cosAngle, atmosphere.mieAnisotropy);
    float stepLength = rayLength / float(steps);
    for (int i = 0; i < steps; ++i) {
        vec3 position = origin + direction * (float(i) + YQLORA_MARCH_SAMPLE_OFFSET) * stepLength;
        float radius = length(position);
        AtmosphereMedium medium = atmosphereMedium(radius - atmosphere.bottomRadius);
        vec3 sampleTransmittance = exp(-medium.extinction * stepLength);
        vec3 sunTransmittance = transmittanceToSun(position, toSun);
        vec3 source = sunTransmittance * (medium.rayleigh * rayleigh + medium.mie * mie);
#ifdef YQLORA_ATMOSPHERE_MULTIPLE
        source += multipleScattering(radius, dot(position / radius, toSun)) * medium.scattering;
#endif
        // Интеграл по шагу при постоянной среде (Hillaire, 2020): без него
        // при густой среде и крупном шаге свет терял бы энергию.
        vec3 kept = (vec3(1.0) - sampleTransmittance) / max(medium.extinction, vec3(YQLORA_ATMOSPHERE_EPSILON));
        result.luminance += result.transmittance * source * kept;
        result.transfer += result.transmittance * medium.scattering * kept;
        result.transmittance *= sampleTransmittance;
    }
    if (ground && hitsGround) {
        vec3 position = origin + direction * toGround;
        vec3 normal = normalize(position);
        float cosSun = dot(normal, toSun);
        vec3 sunTransmittance = transmittanceToTop(length(position), cosSun);
        result.luminance += result.transmittance * sunTransmittance * max(cosSun, 0.0) * atmosphere.groundAlbedo
                            / YQLORA_ATMOSPHERE_PI;
    }

    return result;
}

#endif
