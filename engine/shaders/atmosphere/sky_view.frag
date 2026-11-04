#version 450

// Таблица вида неба: всё небо вокруг камеры вида для её высоты и солнца.
#define YQLORA_ATMOSPHERE_MULTIPLE
#include "march.glsl"

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

const int kSteps = 30;
const float kNoLimit = 1e9;

void main()
{
    vec3 origin = atmospherePosition(view.cameraPosition);
    float radius = length(origin);
    vec3 up = origin / radius;
    vec3 toSun = atmosphereToSun();
    // Базис вокруг вертикали: азимут считается от направления на солнце.
    vec3 sunAcross = toSun - up * dot(toSun, up);
    sunAcross = dot(sunAcross, sunAcross) > YQLORA_ATMOSPHERE_EPSILON ? normalize(sunAcross)
                                                                      : normalize(cross(up, vec3(0.0, 0.0, 1.0)));
    vec3 side = cross(up, sunAcross);

    float cosZenith = 0.0;
    float cosAzimuth = 0.0;
    skyViewParameters(vUv, radius, cosZenith, cosAzimuth);
    float sinZenith = sqrt(max(1.0 - cosZenith * cosZenith, 0.0));
    float sinAzimuth = sqrt(max(1.0 - cosAzimuth * cosAzimuth, 0.0));
    vec3 direction = up * cosZenith + sinZenith * (sunAcross * cosAzimuth + side * sinAzimuth);

    AtmosphereMarch march = marchAtmosphere(origin, direction, toSun, kNoLimit, kSteps, false, true);
    fragColor = vec4(march.luminance, 1.0);
}
