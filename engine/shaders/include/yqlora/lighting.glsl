// Освещение PBR metallic-roughness, как в glTF (brdf.glsl): источники кадра,
// свет и отражения неба, свечение, дымка. Всё в линейном RGB и в физических
// единицах: свет в люксах и канделах, результат - яркость в нитах.
#ifndef YQLORA_LIGHTING_GLSL
#define YQLORA_LIGHTING_GLSL

#include <yqlora/atmosphere.glsl>
#include <yqlora/brdf.glsl>
#include <yqlora/exposure.glsl>
#include <yqlora/harmonics.glsl>
#include <yqlora/shadows.glsl>
#include <yqlora/uniforms.glsl>

// Защита от деления на ноль у источника.
const float YQLORA_MIN_DISTANCE_SQUARED = 0.0001;

// Свет неба (EnvironmentLighting в C++): рассеянный - 9 коэффициентов
// гармоник (пиксель i - коэффициент i), отражённый - куб с мипами по
// шероховатости и таблица BRDF (split-sum).
layout(set = 0, binding = 3) uniform sampler2D ambientHarmonics;
layout(set = 0, binding = 4) uniform samplerCube environmentSpecular;
layout(set = 0, binding = 5) uniform sampler2D environmentBrdf;

// Что поверхность в точке: шейдер заполняет, shade() освещает.
struct Surface {
    // В мире.
    vec3 position;
    // В мире, единичная.
    vec3 normal;
    // Линейный цвет.
    vec3 baseColor;
    float alpha;
    float metallic;
    float roughness;
    // Затенение света и отражений неба: 1 - не затенено. Прямой свет не гасит.
    float occlusion;
    // Свечение: яркость в нитах по каналам.
    vec3 emission;
    // Насколько свечение подчиняется экспозиции камеры: 1 - физически, 0 -
    // на экране всегда той же яркости, что задана.
    float emissionExposureWeight;
};

// Белая неметаллическая поверхность средней шероховатости.
Surface defaultSurface(vec3 position, vec3 normal)
{
    Surface surface;
    surface.position = position;
    surface.normal = normal;
    surface.baseColor = vec3(1.0);
    surface.alpha = 1.0;
    surface.metallic = 0.0;
    surface.roughness = 0.5;
    surface.occlusion = 1.0;
    surface.emission = vec3(0.0);
    surface.emissionExposureWeight = 1.0;

    return surface;
}

// Яркость в нитах от одного источника: toLight - единичное направление к
// нему, illuminance - освещённость от него в люксах на площадке, повёрнутой к
// нему. Белая рассеивающая поверхность, повёрнутая к источнику, получает
// illuminance / пи.
vec3 lightContribution(Surface surface, vec3 toView, vec3 toLight, vec3 illuminance)
{
    float cosNormalLight = dot(surface.normal, toLight);
    if (cosNormalLight <= 0.0) {
        return vec3(0.0);
    }
    vec3 halfway = normalize(toView + toLight);
    float cosNormalView = max(dot(surface.normal, toView), YQLORA_MIN_COSINE);
    float cosNormalHalf = max(dot(surface.normal, halfway), 0.0);
    float cosViewHalf = max(dot(toView, halfway), 0.0);
    float roughness = max(surface.roughness, YQLORA_MIN_ROUGHNESS);
    float alpha = roughness * roughness;

    vec3 f0 = mix(YQLORA_DIELECTRIC_F0, surface.baseColor, surface.metallic);
    vec3 fresnel = fresnelSchlick(cosViewHalf, f0);
    vec3 specular = distributionGgx(cosNormalHalf, alpha) * visibilitySmith(cosNormalView, cosNormalLight, alpha)
                    * fresnel;
    vec3 diffuse = (1.0 - fresnel) * (1.0 - surface.metallic) * surface.baseColor / YQLORA_PI;

    return (diffuse + specular) * illuminance * cosNormalLight;
}

// Плавное окно до нуля на дальности, как в glTF KHR_lights_punctual.
float rangeWindow(float distanceToLight, float range)
{
    float ratio = distanceToLight / range;
    float window = clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);

    return window * window;
}

// Окружающий свет, приходящий на поверхность с нормалью normal (единичной):
// освещённость от неба, делённая на пи, то есть яркость белой рассеивающей
// поверхности. Не меньше нуля: срезанный ряд гармоник у яркого неба может
// уйти ниже.
vec3 ambientLight(vec3 normal)
{
    vec3 light = vec3(0.0);
    for (int i = 0; i < YQLORA_HARMONICS_COUNT; ++i) {
        light += texelFetch(ambientHarmonics, ivec2(i, 0), 0).rgb * harmonicsCosineBand(i) * harmonicsBasis(i, normal);
    }

    return max(light, vec3(0.0)) * frame.ambientIntensity;
}

// Свет неба на поверхности: рассеянный из гармоник и отражённый из куба по
// шероховатости, с поправкой на многократное отражение в шероховатом
// металле (Fdez-Aguera, 2019), иначе он темнеет. Затенение гасит оба.
vec3 environmentContribution(Surface surface, vec3 toView)
{
    float cosNormalView = max(dot(surface.normal, toView), YQLORA_MIN_COSINE);
    float roughness = max(surface.roughness, YQLORA_MIN_ROUGHNESS);
    vec3 f0 = mix(YQLORA_DIELECTRIC_F0, surface.baseColor, surface.metallic);
    vec2 scaleBias = texture(environmentBrdf, vec2(cosNormalView, roughness)).rg;
    // Френель с поправкой на шероховатость: у шероховатой край светлеет меньше.
    vec3 edge = max(vec3(1.0 - roughness), f0) - f0;
    vec3 fresnel = f0 + edge * pow(1.0 - cosNormalView, 5.0);
    vec3 singleScatter = fresnel * scaleBias.x + scaleBias.y;
    float singleEnergy = scaleBias.x + scaleBias.y;
    float multiEnergy = 1.0 - singleEnergy;
    vec3 averageFresnel = f0 + (1.0 - f0) / 21.0;
    vec3 multiScatter = singleScatter * averageFresnel / (1.0 - multiEnergy * averageFresnel);

    vec3 irradiance = ambientLight(surface.normal);
    vec3 reflected = reflect(-toView, surface.normal);
    vec3 radiance = textureLod(environmentSpecular, reflected, roughness * frame.environmentMaxMip).rgb
                    * frame.ambientIntensity;
    vec3 specular = singleScatter * radiance + multiScatter * multiEnergy * irradiance;
    vec3 diffuseShare = (1.0 - singleScatter - multiScatter * multiEnergy) * (1.0 - surface.metallic);
    vec3 diffuse = diffuseShare * surface.baseColor * irradiance;

    return (diffuse + specular) * surface.occlusion;
}

// Освещённый цвет поверхности: свет неба, все источники кадра (солнце -
// сквозь атмосферу) со своими тенями (окружающий свет тени не гасят),
// свечение и дымка по пути до камеры; с экспозицией вида,
// готовый для HDR-цели. Альфа - из поверхности.
vec4 shade(Surface surface)
{
    vec3 toView = normalize(view.cameraPosition - surface.position);
    vec3 color = environmentContribution(surface, toView);
    for (int i = 0; i < frame.directionalCount; ++i) {
        DirectionalLight light = frame.directionalLights[i];
        vec3 illuminance = light.color;
        if (i == 0 && frame.hasSun != 0) {
            illuminance *= atmosphereTransmittance(surface.position, -light.direction)
                           * sunShadow(surface.position, surface.normal, -light.direction);
        }
        color += lightContribution(surface, toView, -light.direction, illuminance);
    }
    for (int i = 0; i < frame.punctualCount; ++i) {
        PunctualLight light = frame.punctualLights[i];
        vec3 offset = light.position - surface.position;
        float distanceSquared = max(dot(offset, offset), YQLORA_MIN_DISTANCE_SQUARED);
        vec3 toLight = offset * inversesqrt(distanceSquared);
        float cone = clamp(dot(light.direction, -toLight) * light.angleScale + light.angleOffset, 0.0, 1.0);
        float attenuation = rangeWindow(sqrt(distanceSquared), light.range) * cone * cone / distanceSquared;
        if (attenuation > 0.0) {
            attenuation *= punctualShadow(i, surface.position, surface.normal, light.position);
            color += lightContribution(surface, toView, toLight, light.color * attenuation);
        }
    }

    // Свечение с весом 0 делится на экспозицию, которую потом получит всё:
    // так на экране остаётся его заданная яркость.
    float emissionScale = mix(1.0 / max(view.exposure, YQLORA_MIN_EXPOSURE), 1.0, surface.emissionExposureWeight);
    color = applyAerialPerspective(color + surface.emission * emissionScale, surface.position);
    color *= shadowCascadeTint(surface.position);

    return vec4(exposed(color), surface.alpha);
}

#endif
