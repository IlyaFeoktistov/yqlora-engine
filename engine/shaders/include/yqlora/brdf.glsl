// Отражение PBR metallic-roughness, как в glTF: блик GGX с видимостью Смита
// и Френелем Шлика. Общее для освещения (lighting.glsl) и для таблиц
// отражений неба, которые строит движок.
#ifndef YQLORA_BRDF_GLSL
#define YQLORA_BRDF_GLSL

const float YQLORA_PI = 3.14159265358979;
// Френель при взгляде прямо у неметаллов: 4 процента, как у большинства
// диэлектриков.
const vec3 YQLORA_DIELECTRIC_F0 = vec3(0.04);
// Совсем гладкая поверхность даёт бесконечно узкий блик, который мерцает.
const float YQLORA_MIN_ROUGHNESS = 0.045;
// Защита от деления на ноль при взгляде вдоль поверхности.
const float YQLORA_MIN_COSINE = 0.0001;

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

// Направление половинного вектора по распределению GGX для случайной точки
// xi (0..1, 0..1), в касательном пространстве (z - нормаль).
vec3 importanceSampleGgx(vec2 xi, float alpha)
{
    float phi = 2.0 * YQLORA_PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// Точка i из count по последовательности Хаммерсли: равномерно, без сгустков.
vec2 hammersley(uint i, uint count)
{
    uint bits = i;
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return vec2(float(i) / float(count), float(bits) * 2.3283064365386963e-10);
}

// Базис вокруг нормали: столбцы - две касательные и нормаль.
mat3 tangentBasis(vec3 normal)
{
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));

    return mat3(tangent, cross(normal, tangent), normal);
}

#endif
