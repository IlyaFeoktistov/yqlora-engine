// Атмосфера планеты по методу Hillaire ("A Scalable and Production Ready Sky
// and Atmosphere Rendering Technique", 2020): Рэлей, Ми и озон, таблицы
// пропускания, многократного рассеяния, вида неба и воздушной перспективы.
// Здесь всё, что о них знают шейдеры: параметры, среда, развёртки таблиц
// (одни и те же для прохода, который таблицу пишет, и для того, кто читает)
// и функции для шейдеров материалов и неба.
//
// Единицы: расстояния в км, коэффициенты на км. Начало координат сцены
// лежит на земле планеты, ось Y - вверх. Значения таблиц - на единицу
// освещённости от солнца: функции ниже умножают их на frame.sunColor.
#ifndef YQLORA_ATMOSPHERE_GLSL
#define YQLORA_ATMOSPHERE_GLSL

#include <yqlora/uniforms.glsl>

// Повторяет AtmosphereUniforms в EngineUniforms.hpp.
layout(set = 0, binding = 1, std140) uniform Atmosphere {
    vec3 rayleighScattering;
    float bottomRadius;
    vec3 mieScattering;
    float topRadius;
    vec3 mieExtinction;
    float mieAnisotropy;
    vec3 ozoneAbsorption;
    float rayleighDensityScale;
    // Линейный.
    vec3 groundAlbedo;
    float mieDensityScale;
    int enabled;
    // Сила воздушной перспективы: 0 - без дымки, 1 - физически.
    float aerialPerspective;
} atmosphere;

// Пропускание до верха атмосферы по высоте и углу к зениту.
layout(set = 0, binding = 2) uniform sampler2D transmittanceLut;
// Небо вокруг камеры вида и воздушная перспектива вдоль её взгляда.
layout(set = 1, binding = 1) uniform sampler2D skyViewLut;
layout(set = 1, binding = 2) uniform sampler2D aerialPerspectiveLut;

const float YQLORA_ATMOSPHERE_PI = 3.14159265358979;
const float YQLORA_METRES_PER_KM = 1000.0;
// Камера и точки сцены не опускаются ниже метра над землёй: на самой
// поверхности луч к горизонту упирается в планету.
const float YQLORA_ATMOSPHERE_MIN_HEIGHT = 0.001;
// Озон - слой-треугольник: полная плотность на 25 км, ноль на 10 и 40.
const float YQLORA_OZONE_CENTER = 25.0;
const float YQLORA_OZONE_HALF_WIDTH = 15.0;
// Воздушная перспектива считается до этого расстояния, дальше - как на нём.
const float YQLORA_AERIAL_DISTANCE = 32.0;
const float YQLORA_ATMOSPHERE_EPSILON = 1e-6;

// Точка мира (метры) в координатах планеты (км, центр планеты в нуле).
vec3 atmospherePosition(vec3 world)
{
    vec3 position = world / YQLORA_METRES_PER_KM;
    position.y = max(position.y, YQLORA_ATMOSPHERE_MIN_HEIGHT);

    return position + vec3(0.0, atmosphere.bottomRadius, 0.0);
}

// Ближайшее пересечение луча (direction единичное) со сферой радиуса radius
// с центром в нуле на расстоянии не меньше нуля; -1 - не пересекает.
float raySphere(vec3 origin, vec3 direction, float radius)
{
    float b = dot(origin, direction);
    float c = dot(origin, origin) - radius * radius;
    float discriminant = b * b - c;
    if (discriminant < 0.0) {
        return -1.0;
    }
    float root = sqrt(discriminant);
    float nearHit = -b - root;
    float farHit = -b + root;
    if (nearHit >= 0.0) {
        return nearHit;
    }

    return farHit >= 0.0 ? farHit : -1.0;
}

// Среда на высоте height (км над землёй).
struct AtmosphereMedium {
    vec3 rayleigh;
    vec3 mie;
    // Рассеяние всего: Рэлей и Ми.
    vec3 scattering;
    // Рассеяние и поглощение.
    vec3 extinction;
};

AtmosphereMedium atmosphereMedium(float height)
{
    float rayleighDensity = exp(height * atmosphere.rayleighDensityScale);
    float mieDensity = exp(height * atmosphere.mieDensityScale);
    float ozoneDensity = max(0.0, 1.0 - abs(height - YQLORA_OZONE_CENTER) / YQLORA_OZONE_HALF_WIDTH);
    AtmosphereMedium medium;
    medium.rayleigh = atmosphere.rayleighScattering * rayleighDensity;
    medium.mie = atmosphere.mieScattering * mieDensity;
    medium.scattering = medium.rayleigh + medium.mie;
    medium.extinction = medium.rayleigh + atmosphere.mieExtinction * mieDensity
                        + atmosphere.ozoneAbsorption * ozoneDensity;

    return medium;
}

float rayleighPhase(float cosAngle)
{
    return 3.0 / (16.0 * YQLORA_ATMOSPHERE_PI) * (1.0 + cosAngle * cosAngle);
}

// Корнетт и Шэнкс: Хеньи и Гринстейн, поправленная под рассеяние Ми.
float miePhase(float cosAngle, float g)
{
    float g2 = g * g;
    float denominator = pow(max(1.0 + g2 - 2.0 * g * cosAngle, YQLORA_ATMOSPHERE_EPSILON), 1.5);

    return 3.0 / (8.0 * YQLORA_ATMOSPHERE_PI) * (1.0 - g2) * (1.0 + cosAngle * cosAngle) / ((2.0 + g2) * denominator);
}

// Развёртка таблицы пропускания (Bruneton): по высоте радиуса radius и
// косинусу угла к зениту. Строки - высота, столбцы - расстояние до верха.
vec2 transmittanceUv(float radius, float cosZenith)
{
    float bottom2 = atmosphere.bottomRadius * atmosphere.bottomRadius;
    float horizon = sqrt(max(atmosphere.topRadius * atmosphere.topRadius - bottom2, 0.0));
    float rho = sqrt(max(radius * radius - bottom2, 0.0));
    float discriminant = radius * radius * (cosZenith * cosZenith - 1.0) + atmosphere.topRadius * atmosphere.topRadius;
    float distanceToTop = max(0.0, -radius * cosZenith + sqrt(max(discriminant, 0.0)));
    float minDistance = atmosphere.topRadius - radius;
    float maxDistance = rho + horizon;

    return vec2((distanceToTop - minDistance) / max(maxDistance - minDistance, YQLORA_ATMOSPHERE_EPSILON), rho / horizon);
}

void transmittanceParameters(vec2 uv, out float radius, out float cosZenith)
{
    float bottom2 = atmosphere.bottomRadius * atmosphere.bottomRadius;
    float horizon = sqrt(max(atmosphere.topRadius * atmosphere.topRadius - bottom2, 0.0));
    float rho = horizon * uv.y;
    radius = sqrt(rho * rho + bottom2);
    float minDistance = atmosphere.topRadius - radius;
    float maxDistance = rho + horizon;
    float distanceToTop = minDistance + uv.x * (maxDistance - minDistance);
    cosZenith = distanceToTop < YQLORA_ATMOSPHERE_EPSILON
                    ? 1.0
                    : (horizon * horizon - rho * rho - distanceToTop * distanceToTop) / (2.0 * radius * distanceToTop);
    cosZenith = clamp(cosZenith, -1.0, 1.0);
}

// Пропускание от точки на радиусе radius до верха атмосферы под углом к
// зениту; планету, которая может оказаться на пути, не учитывает.
vec3 transmittanceToTop(float radius, float cosZenith)
{
    return texture(transmittanceLut, transmittanceUv(radius, cosZenith)).rgb;
}

// Пропускание от точки к солнцу: ноль, если солнце за планетой.
vec3 transmittanceToSun(vec3 position, vec3 toSun)
{
    if (raySphere(position, toSun, atmosphere.bottomRadius) >= 0.0) {
        return vec3(0.0);
    }
    float radius = length(position);

    return transmittanceToTop(radius, dot(position / radius, toSun));
}

// Развёртка таблицы вида неба: по углу к зениту (ближе к горизонту - больше
// строк: там небо меняется сильнее всего) и по азимуту от солнца, 0..пи
// (небо симметрично плоскости солнца; у солнца - больше столбцов).
vec2 skyViewUv(float radius, float cosZenith, float cosAzimuth)
{
    float horizonDistance = sqrt(max(radius * radius - atmosphere.bottomRadius * atmosphere.bottomRadius, 0.0));
    float beta = acos(clamp(horizonDistance / radius, -1.0, 1.0));
    float zenithHorizonAngle = YQLORA_ATMOSPHERE_PI - beta;
    float zenithAngle = acos(clamp(cosZenith, -1.0, 1.0));
    float v = 0.0;
    if (zenithAngle < zenithHorizonAngle) {
        float c = zenithAngle / zenithHorizonAngle;
        v = (1.0 - sqrt(max(1.0 - c, 0.0))) * 0.5;
    } else {
        float c = (zenithAngle - zenithHorizonAngle) / max(beta, YQLORA_ATMOSPHERE_EPSILON);
        v = 0.5 + sqrt(max(c, 0.0)) * 0.5;
    }
    float u = sqrt(acos(clamp(cosAzimuth, -1.0, 1.0)) / YQLORA_ATMOSPHERE_PI);

    return vec2(u, v);
}

void skyViewParameters(vec2 uv, float radius, out float cosZenith, out float cosAzimuth)
{
    float horizonDistance = sqrt(max(radius * radius - atmosphere.bottomRadius * atmosphere.bottomRadius, 0.0));
    float beta = acos(clamp(horizonDistance / radius, -1.0, 1.0));
    float zenithHorizonAngle = YQLORA_ATMOSPHERE_PI - beta;
    float zenithAngle = 0.0;
    if (uv.y < 0.5) {
        float c = 1.0 - 2.0 * uv.y;
        zenithAngle = zenithHorizonAngle * (1.0 - c * c);
    } else {
        float c = 2.0 * uv.y - 1.0;
        zenithAngle = zenithHorizonAngle + beta * c * c;
    }
    cosZenith = cos(zenithAngle);
    cosAzimuth = cos(uv.x * uv.x * YQLORA_ATMOSPHERE_PI);
}

// Куда светит солнце кадра; без солнца - сверху, чтобы базис неба не выродился.
vec3 atmosphereToSun()
{
    return frame.hasSun != 0 ? -frame.sunDirection : vec3(0.0, 1.0, 0.0);
}

// Яркость неба в нитах в направлении direction от камеры вида: из таблицы
// вида неба. Без атмосферы или без солнца - чёрное.
vec3 atmosphereSky(vec3 direction)
{
    if (atmosphere.enabled == 0 || frame.hasSun == 0) {
        return vec3(0.0);
    }
    vec3 origin = atmospherePosition(view.cameraPosition);
    float radius = length(origin);
    vec3 up = origin / radius;
    vec3 toSun = atmosphereToSun();
    vec3 sunAcross = toSun - up * dot(toSun, up);
    vec3 viewAcross = direction - up * dot(direction, up);
    // Солнце или взгляд в зените: азимут любой, небо вокруг одинаковое.
    float cosAzimuth = 1.0;
    if (dot(sunAcross, sunAcross) > YQLORA_ATMOSPHERE_EPSILON && dot(viewAcross, viewAcross) > YQLORA_ATMOSPHERE_EPSILON) {
        cosAzimuth = dot(normalize(sunAcross), normalize(viewAcross));
    }
    vec2 uv = skyViewUv(radius, dot(direction, up), cosAzimuth);

    return texture(skyViewLut, uv).rgb * frame.sunColor;
}

// Пропускание атмосферы от точки мира position в направлении direction
// до её верха: ноль, если по пути планета; без атмосферы - 1.
vec3 atmosphereTransmittance(vec3 position, vec3 direction)
{
    if (atmosphere.enabled == 0) {
        return vec3(1.0);
    }

    return transmittanceToSun(atmospherePosition(position), direction);
}

// Число срезов воздушной перспективы: квадратные срезы лежат в одну строку.
float aerialSliceCount()
{
    vec2 size = vec2(textureSize(aerialPerspectiveLut, 0));

    return size.x / size.y;
}

// Срез slice атласа в точке экрана uv: рассеяние и среднее пропускание.
vec4 aerialSlice(float slice, vec2 uv)
{
    vec2 size = vec2(textureSize(aerialPerspectiveLut, 0));
    float count = size.x / size.y;
    // Внутри своего среза: линейная выборка не берёт соседний.
    float halfTexel = 0.5 / size.y;
    float u = (slice + clamp(uv.x, halfTexel, 1.0 - halfTexel)) / count;

    return texture(aerialPerspectiveLut, vec2(u, uv.y));
}

// Расстояние до середины среза slice (км): срезы гуще у камеры.
float aerialSliceDistance(float slice, float count)
{
    float share = (slice + 0.5) / count;

    return YQLORA_AERIAL_DISTANCE * share * share;
}

// Цвет поверхности (нит) в точке мира position, как его видно сквозь
// воздух от камеры вида: ослабленный по пути и с рассеянным светом неба.
vec3 applyAerialPerspective(vec3 color, vec3 position)
{
    if (atmosphere.enabled == 0 || atmosphere.aerialPerspective <= 0.0) {
        return color;
    }
    vec4 clip = view.viewProjection * vec4(position, 1.0);
    vec2 uv = clamp(clip.xy / clip.w * 0.5 + 0.5, 0.0, 1.0);
    float count = aerialSliceCount();
    float distanceKm = min(length(position - view.cameraPosition) / YQLORA_METRES_PER_KM, YQLORA_AERIAL_DISTANCE);
    float slice = sqrt(distanceKm / YQLORA_AERIAL_DISTANCE) * count - 0.5;
    vec4 scattering;
    if (slice < 0.0) {
        // Ближе первого среза: от чистого воздуха у камеры к первому срезу.
        scattering = mix(vec4(0.0, 0.0, 0.0, 1.0), aerialSlice(0.0, uv), slice + 1.0);
    } else {
        float first = min(floor(slice), count - 1.0);
        float second = min(first + 1.0, count - 1.0);
        scattering = mix(aerialSlice(first, uv), aerialSlice(second, uv), slice - first);
    }

    // Сила больше 1 гуще физической: пропускание - в её степени, рассеяние -
    // столько же раз.
    float strength = atmosphere.aerialPerspective;

    return color * pow(scattering.a, strength) + scattering.rgb * frame.sunColor * strength;
}

#endif
