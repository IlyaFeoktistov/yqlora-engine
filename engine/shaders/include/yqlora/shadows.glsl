// Тени источников (ShadowPass в C++): каскады солнца и атлас плиток
// точечных и прожекторов. Hard - одно сравнение глубины (с линейным
// сэмплером - по четырём текселям), Soft - полутень по размеру источника
// (PCSS): средняя глубина заслоняющих вокруг точки, из неё ширина полутени,
// затем фильтр такой ширины. Отсчёты лежат на диске Фогеля (золотой угол),
// повёрнутом по пикселю: ровно при любом их числе и без полос.
#ifndef YQLORA_SHADOWS_GLSL
#define YQLORA_SHADOWS_GLSL

#include <yqlora/uniforms.glsl>

// Повторяют константы FrameData.hpp.
#define YQLORA_MAX_SHADOW_CASCADES 4
#define YQLORA_MAX_SHADOW_TILES 16
#define YQLORA_SHADOW_ATLAS_COLUMNS 4
#define YQLORA_SHADOW_NEAR 0.05
#define YQLORA_SHADOW_HARD 1
#define YQLORA_SHADOW_SOFT 2

// Повторяет ShadowUniforms в EngineUniforms.hpp.
layout(set = 1, binding = 3, std140) uniform Shadows {
    mat4 cascadeMatrices[YQLORA_MAX_SHADOW_CASCADES];
    mat4 tileMatrices[YQLORA_MAX_SHADOW_TILES];
    vec4 cascadeSplits;
    vec4 cascadeWidths;
    vec4 cascadeDepthRanges;
    vec3 cameraForward;
    int cascadeCount;
    int sunShadows;
    float sunShadowStrength;
    float sunShadowSoftness;
    float sunShadowBias;
    float sunShadowNormalBias;
    float sunSourceTangent;
    int blockerSamples;
    int filterSamples;
    float maxPenumbra;
    float cascadeSize;
    float tileSize;
    int showCascades;
    ivec4 punctualShadows[YQLORA_MAX_PUNCTUAL_LIGHTS];
    vec4 punctualShadowParams[YQLORA_MAX_PUNCTUAL_LIGHTS];
    vec4 punctualShadowSizes[YQLORA_MAX_PUNCTUAL_LIGHTS];
} shadows;

layout(set = 1, binding = 4) uniform sampler2DShadow shadowCascades;
layout(set = 1, binding = 5) uniform sampler2D shadowCascadesDepth;
layout(set = 1, binding = 6) uniform sampler2DShadow shadowAtlas;
layout(set = 1, binding = 7) uniform sampler2D shadowAtlasDepth;

// Доля дальнего края каскада, где он смешивается со следующим, а последний -
// гаснет до "без тени".
const float YQLORA_CASCADE_BLEND = 0.1;
const float YQLORA_GOLDEN_ANGLE = 2.39996323;

// Место карты в общей текстуре: угол и размер в координатах текстуры.
struct ShadowRect {
    vec2 origin;
    float size;
    // Тексель общей текстуры в её координатах.
    float texel;
};

// Отсчёт index из count на единичном диске, повёрнутом на rotation.
vec2 vogelDisc(int index, int count, float rotation)
{
    float radius = sqrt((float(index) + 0.5) / float(count));
    float angle = float(index) * YQLORA_GOLDEN_ANGLE + rotation;

    return radius * vec2(cos(angle), sin(angle));
}

// Поворот диска по пикселю: шум с градиентом между соседями (Jimenez,
// 2014). Освещение собирается и в вершинной стадии, где пикселя нет.
float shadowRotation()
{
#ifdef YQLORA_FRAGMENT
    return 6.28318531 * fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
#else
    return 0.0;
#endif
}

// Точка карты uv (0..1 внутри карты) в координатах общей текстуры, не ближе
// полутекселя к краю карты: фильтр не заходит на соседнюю.
vec2 shadowUv(ShadowRect rect, vec2 uv)
{
    vec2 inside = rect.origin + uv * rect.size;
    float margin = rect.texel * 0.5;

    return clamp(inside, rect.origin + margin, rect.origin + rect.size - margin);
}

// Средняя глубина заслоняющих (ближе depth) в радиусе radius (доля карты)
// вокруг uv; отрицательная - заслоняющих нет.
float findBlockers(sampler2D depthMap, ShadowRect rect, vec2 uv, float depth, float radius, float rotation)
{
    float sum = 0.0;
    int found = 0;
    for (int i = 0; i < shadows.blockerSamples; ++i) {
        vec2 at = uv + vogelDisc(i, shadows.blockerSamples, rotation) * radius;
        float stored = texture(depthMap, shadowUv(rect, at)).r;
        if (stored < depth) {
            sum += stored;
            ++found;
        }
    }

    return found > 0 ? sum / float(found) : -1.0;
}

// Доля света, дошедшего до точки: среднее сравнений на диске радиуса radius.
float filterShadow(sampler2DShadow map, ShadowRect rect, vec2 uv, float depth, float radius, float rotation)
{
    float lit = 0.0;
    for (int i = 0; i < shadows.filterSamples; ++i) {
        vec2 at = uv + vogelDisc(i, shadows.filterSamples, rotation) * radius;
        lit += texture(map, vec3(shadowUv(rect, at), depth));
    }

    return lit / float(shadows.filterSamples);
}

// Каскад cascade: его место в карте каскадов (2 x 2, один каскад - вся карта).
ShadowRect cascadeRect(int cascade)
{
    int columns = shadows.cascadeCount > 1 ? 2 : 1;
    float size = 1.0 / float(columns);
    ShadowRect rect;
    rect.origin = vec2(float(cascade % columns), float(cascade / columns)) * size;
    rect.size = size;
    rect.texel = size / shadows.cascadeSize;

    return rect;
}

// Свет солнца в точке по каскаду cascade (1 - не затенено).
float sunCascadeShadow(int cascade, vec3 position, vec3 normal, vec3 toLight)
{
    float width = shadows.cascadeWidths[cascade];
    float texelMeters = width / shadows.cascadeSize;
    vec3 shifted = position + normal * (shadows.sunShadowNormalBias * texelMeters)
                   + toLight * (shadows.sunShadowBias * texelMeters);
    vec4 clip = shadows.cascadeMatrices[cascade] * vec4(shifted, 1.0);
    vec3 coord = clip.xyz / clip.w;
    vec2 uv = coord.xy * 0.5 + 0.5;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || coord.z > 1.0) {
        return 1.0;
    }
    ShadowRect rect = cascadeRect(cascade);
    if (shadows.sunShadows != YQLORA_SHADOW_SOFT) {
        return texture(shadowCascades, vec3(shadowUv(rect, uv), coord.z));
    }

    // Доля карты на метр и предел полутени в долях карты.
    float perMeter = 1.0 / width;
    float maxRadius = shadows.maxPenumbra * 0.5 / shadows.cascadeSize;
    float rotation = shadowRotation();
    float blocker = findBlockers(shadowCascadesDepth, rect, uv, coord.z, maxRadius, rotation);
    if (blocker < 0.0) {
        return 1.0;
    }
    // Глубина карты линейна: разница в метрах - разница глубин на глубину коробки.
    float gap = (coord.z - blocker) * shadows.cascadeDepthRanges[cascade];
    float penumbra = 2.0 * shadows.sunSourceTangent * gap * shadows.sunShadowSoftness * perMeter;
    float radius = clamp(penumbra * 0.5, 0.5 / shadows.cascadeSize, maxRadius);

    return filterShadow(shadowCascades, rect, uv, coord.z, radius, rotation);
}

// Каскад точки по глубине вида; cascadeCount - дальше теней.
int shadowCascadeOf(vec3 position)
{
    float depth = dot(position - view.cameraPosition, shadows.cameraForward);
    int cascade = 0;
    while (cascade < shadows.cascadeCount && depth > shadows.cascadeSplits[cascade]) {
        ++cascade;
    }

    return cascade;
}

// Отладочный вид каскадов: цвет каскада точки, вне каскадов и без отладки - белый.
vec3 shadowCascadeTint(vec3 position)
{
    const vec3 tints[YQLORA_MAX_SHADOW_CASCADES] = vec3[](
        vec3(1.0, 0.3, 0.3), vec3(0.3, 1.0, 0.3), vec3(0.3, 0.3, 1.0), vec3(1.0, 1.0, 0.3)
    );
    int cascade = shadowCascadeOf(position);

    return shadows.showCascades != 0 && cascade < shadows.cascadeCount ? tints[cascade] : vec3(1.0);
}

// Свет солнца в точке: каскад по глубине вида, на краю каскада - смесь со
// следующим, у дальности теней - плавно без тени.
float sunShadow(vec3 position, vec3 normal, vec3 toLight)
{
    if (shadows.sunShadows == 0 || shadows.cascadeCount == 0 || object.receiveShadows == 0) {
        return 1.0;
    }
    float depth = dot(position - view.cameraPosition, shadows.cameraForward);
    int cascade = shadowCascadeOf(position);
    if (cascade >= shadows.cascadeCount) {
        return 1.0;
    }
    float splitNear = cascade == 0 ? 0.0 : shadows.cascadeSplits[cascade - 1];
    float splitFar = shadows.cascadeSplits[cascade];
    float blendStart = mix(splitNear, splitFar, 1.0 - YQLORA_CASCADE_BLEND);
    float blend = clamp((depth - blendStart) / (splitFar - blendStart), 0.0, 1.0);

    float lit = sunCascadeShadow(cascade, position, normal, toLight);
    if (blend > 0.0) {
        float next = cascade + 1 < shadows.cascadeCount ? sunCascadeShadow(cascade + 1, position, normal, toLight)
                                                         : 1.0;
        lit = mix(lit, next, blend);
    }

    return mix(1.0, lit, shadows.sunShadowStrength);
}

// Расстояние от источника по глубине карты перспективной плитки (0..1).
float shadowLinearDepth(float depth, float far)
{
    return YQLORA_SHADOW_NEAR * far / (far - depth * (far - YQLORA_SHADOW_NEAR));
}

// Свет точечного или прожектора номер light (в punctualLights) в точке.
float punctualShadow(int light, vec3 position, vec3 normal, vec3 lightPosition)
{
    ivec4 info = shadows.punctualShadows[light];
    if (info.x < 0 || object.receiveShadows == 0) {
        return 1.0;
    }
    vec4 params = shadows.punctualShadowParams[light];
    vec4 sizes = shadows.punctualShadowSizes[light];
    vec3 fromLight = position - lightPosition;
    int tile = info.x;
    // Точечный: грань куба по наибольшей оси, в порядке +X, -X, +Y, -Y, +Z, -Z.
    if (frame.punctualLights[light].angleScale == 0.0) {
        vec3 axis = abs(fromLight);
        if (axis.x >= axis.y && axis.x >= axis.z) {
            tile += fromLight.x > 0.0 ? 0 : 1;
        } else if (axis.y >= axis.z) {
            tile += fromLight.y > 0.0 ? 2 : 3;
        } else {
            tile += fromLight.z > 0.0 ? 4 : 5;
        }
    }
    float distanceToLight = length(fromLight);
    vec3 toLight = -fromLight / max(distanceToLight, 0.0001);
    // Тексель плитки в метрах на этом расстоянии.
    float texelMeters = 2.0 * distanceToLight / (sizes.y * shadows.tileSize);
    vec3 shifted = position + normal * (params.w * texelMeters) + toLight * (params.z * texelMeters);
    vec4 clip = shadows.tileMatrices[tile] * vec4(shifted, 1.0);
    vec3 coord = clip.xyz / clip.w;
    vec2 uv = coord.xy * 0.5 + 0.5;
    if (clip.w <= 0.0 || any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || coord.z > 1.0) {
        return 1.0;
    }
    float tileShare = 1.0 / float(YQLORA_SHADOW_ATLAS_COLUMNS);
    ShadowRect rect;
    rect.origin = vec2(float(tile % YQLORA_SHADOW_ATLAS_COLUMNS), float(tile / YQLORA_SHADOW_ATLAS_COLUMNS))
                  * tileShare;
    rect.size = tileShare;
    rect.texel = tileShare / shadows.tileSize;

    float lit;
    if (info.y != YQLORA_SHADOW_SOFT) {
        lit = texture(shadowAtlas, vec3(shadowUv(rect, uv), coord.z));
    } else {
        float maxRadius = shadows.maxPenumbra * 0.5 / shadows.tileSize;
        float rotation = shadowRotation();
        float blocker = findBlockers(shadowAtlasDepth, rect, uv, coord.z, maxRadius, rotation);
        if (blocker < 0.0) {
            return 1.0;
        }
        float receiverDistance = shadowLinearDepth(coord.z, sizes.z);
        float blockerDistance = shadowLinearDepth(blocker, sizes.z);
        // Полутень растёт с расстоянием от заслоняющего и с размером источника.
        float penumbraMeters = 2.0 * sizes.x * (receiverDistance - blockerDistance) / blockerDistance * params.y;
        // Доля плитки на метр на расстоянии точки.
        float perMeter = sizes.y / (2.0 * receiverDistance);
        float radius = clamp(penumbraMeters * perMeter * 0.5, 0.5 / shadows.tileSize, maxRadius);
        lit = filterShadow(shadowAtlas, rect, uv, coord.z, radius, rotation);
    }

    return mix(1.0, lit, params.x);
}

#endif
