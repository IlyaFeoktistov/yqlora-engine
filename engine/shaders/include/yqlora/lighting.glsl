// Освещение PBR metallic-roughness, как в glTF: блик GGX с геометрией Смита
// и Френелем Шлика, рассеяние Ламберта. Всё в линейном RGB и в физических
// единицах: свет в люксах и канделах, результат - яркость в нитах.
#ifndef YQLORA_LIGHTING_GLSL
#define YQLORA_LIGHTING_GLSL

#include <yqlora/uniforms.glsl>

const float YQLORA_PI = 3.14159265358979;
// Френель при взгляде прямо у неметаллов: 4 процента, как у большинства
// диэлектриков.
const vec3 YQLORA_DIELECTRIC_F0 = vec3(0.04);
// Совсем гладкая поверхность даёт бесконечно узкий блик, который мерцает.
const float YQLORA_MIN_ROUGHNESS = 0.045;
// Защита от деления на ноль у источника и при взгляде вдоль поверхности.
const float YQLORA_MIN_DISTANCE_SQUARED = 0.0001;
const float YQLORA_MIN_COSINE = 0.0001;

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
    // Затенение окружающего света: 1 - не затенено. Прямой свет не гасит.
    float occlusion;
    // Свечение: яркость в нитах по каналам.
    vec3 emission;
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

    return surface;
}

float distributionGgx(float cosNormalHalf, float alpha)
{
    float alpha2 = alpha * alpha;
    float d = cosNormalHalf * cosNormalHalf * (alpha2 - 1.0) + 1.0;

    return alpha2 / (YQLORA_PI * d * d);
}

// Видимость Смита с учётом высоты (height-correlated), уже делённая на 4 n.l n.v.
float visibilitySmith(float cosNormalView, float cosNormalLight, float alpha)
{
    float alpha2 = alpha * alpha;
    float viewTerm = cosNormalLight * sqrt(cosNormalView * cosNormalView * (1.0 - alpha2) + alpha2);
    float lightTerm = cosNormalView * sqrt(cosNormalLight * cosNormalLight * (1.0 - alpha2) + alpha2);

    return 0.5 / max(viewTerm + lightTerm, YQLORA_MIN_COSINE);
}

vec3 fresnelSchlick(float cosViewHalf, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosViewHalf, 5.0);
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

// Освещённый цвет поверхности: окружающий свет, все источники кадра,
// свечение; с экспозицией вида, готовый для HDR-цели. Альфа - из поверхности.
vec4 shade(Surface surface)
{
    vec3 toView = normalize(view.cameraPosition - surface.position);
    vec3 color = frame.ambient * surface.baseColor * (1.0 - surface.metallic) * surface.occlusion;
    for (int i = 0; i < frame.directionalCount; ++i) {
        DirectionalLight light = frame.directionalLights[i];
        color += lightContribution(surface, toView, -light.direction, light.color);
    }
    for (int i = 0; i < frame.punctualCount; ++i) {
        PunctualLight light = frame.punctualLights[i];
        vec3 offset = light.position - surface.position;
        float distanceSquared = max(dot(offset, offset), YQLORA_MIN_DISTANCE_SQUARED);
        vec3 toLight = offset * inversesqrt(distanceSquared);
        float cone = clamp(dot(light.direction, -toLight) * light.angleScale + light.angleOffset, 0.0, 1.0);
        float attenuation = rangeWindow(sqrt(distanceSquared), light.range) * cone * cone / distanceSquared;
        if (attenuation > 0.0) {
            color += lightContribution(surface, toView, toLight, light.color * attenuation);
        }
    }

    return vec4((color + surface.emission) * view.exposure, surface.alpha);
}

#endif
