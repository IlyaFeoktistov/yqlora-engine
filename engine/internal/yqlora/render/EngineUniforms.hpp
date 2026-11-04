#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/rhi/RhiTypes.hpp"
#include "yqlora/shader/ShaderProfile.hpp"
#include "yqlora/shader/UniformLayout.hpp"

#include <cstddef>

namespace yqlora {

// Uniform-блоки движка (engine/shaders/include/yqlora/uniforms.glsl) в
// раскладке std140: vec3 занимает 16 байт, матрицы - по столбцам vec4.
// Структуры повторяют её явно; блок каждого шейдера сверяется с раскладками
// ниже при загрузке.
// vec3 и float рядом занимают один vec4: так структуры света без пустот.
struct alignas(16) DirectionalLightUniforms {
    // Куда свет идёт.
    Vec3 direction{0.0f, -1.0f, 0.0f};
    float padding0 = 0.0f;
    Vec3 color{0.0f};
    float padding1 = 0.0f;
};

// Точечный и прожектор. Конус - множитель saturate(cos * angleScale +
// angleOffset), как в glTF KHR_lights_punctual: у точечного scale 0 и
// offset 1, то есть без конуса.
struct alignas(16) PunctualLightUniforms {
    Vec3 position{0.0f};
    float range = 0.0f;
    Vec3 color{0.0f};
    float angleScale = 0.0f;
    Vec3 direction{0.0f, 0.0f, -1.0f};
    float angleOffset = 1.0f;
};

struct alignas(16) FrameUniforms {
    // Куда идёт свет солнца; без солнца hasSun = 0. Солнце - directionalLights[0].
    Vec3 sunDirection{0.0f, -1.0f, 0.0f};
    int hasSun = 0;
    // Освещённость от солнца в люксах по каналам, до атмосферы.
    Vec3 sunColor{0.0f};
    int directionalCount = 0;
    int punctualCount = 0;
    DirectionalLightUniforms directionalLights[kMaxDirectionalLights]{};
    PunctualLightUniforms punctualLights[kMaxPunctualLights]{};
};

struct alignas(16) ViewUniforms {
    Mat4 viewProjection{1.0f};
    // Из точки экрана в мир: по ней небо находит направление пикселя.
    Mat4 inverseViewProjection{1.0f};
    Vec3 cameraPosition{0.0f};
    // Множитель яркости вида (ViewExposure::factor): шейдеры умножают на
    // него всё, что пишут в HDR-цель.
    float exposure = 1.0f;
};

// Атмосфера в единицах шейдера (atmosphere.glsl): км и коэффициенты на км.
// Плотность слоя Рэлея и Ми - exp(высота * densityScale).
struct alignas(16) AtmosphereUniforms {
    Vec3 rayleighScattering{0.0f};
    float bottomRadius = 0.0f;
    Vec3 mieScattering{0.0f};
    float topRadius = 0.0f;
    Vec3 mieExtinction{0.0f};
    float mieAnisotropy = 0.0f;
    Vec3 ozoneAbsorption{0.0f};
    float rayleighDensityScale = 0.0f;
    // Линейный.
    Vec3 groundAlbedo{0.0f};
    float mieDensityScale = 0.0f;
    // 0 - атмосферы нет: небо чёрное, свет солнца не ослабляется, дымки нет.
    int enabled = 0;
    // Сила воздушной перспективы: 0 - без дымки, 1 - физически.
    float aerialPerspective = 0.0f;
};

struct alignas(16) ObjectUniforms {
    Mat4 model{1.0f};
    // mat3 в std140 - три vec4, проще хранить mat4.
    Mat4 normalMatrix{1.0f};
};

// Отступы (padding) в раскладках не перечислены: шейдер их не объявляет.
inline const std::vector<UniformMember> kDirectionalLightMembers{
    YQLORA_UNIFORM_MEMBER(DirectionalLightUniforms, direction),
    YQLORA_UNIFORM_MEMBER(DirectionalLightUniforms, color),
};

inline const std::vector<UniformMember> kPunctualLightMembers{
    YQLORA_UNIFORM_MEMBER(PunctualLightUniforms, position),
    YQLORA_UNIFORM_MEMBER(PunctualLightUniforms, range),
    YQLORA_UNIFORM_MEMBER(PunctualLightUniforms, color),
    YQLORA_UNIFORM_MEMBER(PunctualLightUniforms, angleScale),
    YQLORA_UNIFORM_MEMBER(PunctualLightUniforms, direction),
    YQLORA_UNIFORM_MEMBER(PunctualLightUniforms, angleOffset),
};

inline const UniformLayout kFrameBlock{
    "Frame",
    {kFrameSet, 0},
    sizeof(FrameUniforms),
    {
        YQLORA_UNIFORM_MEMBER(FrameUniforms, sunDirection),
        YQLORA_UNIFORM_MEMBER(FrameUniforms, hasSun),
        YQLORA_UNIFORM_MEMBER(FrameUniforms, sunColor),
        YQLORA_UNIFORM_MEMBER(FrameUniforms, directionalCount),
        YQLORA_UNIFORM_MEMBER(FrameUniforms, punctualCount),
        YQLORA_UNIFORM_STRUCT_MEMBER(FrameUniforms, directionalLights, kDirectionalLightMembers),
        YQLORA_UNIFORM_STRUCT_MEMBER(FrameUniforms, punctualLights, kPunctualLightMembers),
    },
};

inline const UniformLayout kViewBlock{
    "View",
    {kViewSet, 0},
    sizeof(ViewUniforms),
    {
        YQLORA_UNIFORM_MEMBER(ViewUniforms, viewProjection),
        YQLORA_UNIFORM_MEMBER(ViewUniforms, inverseViewProjection),
        YQLORA_UNIFORM_MEMBER(ViewUniforms, cameraPosition),
        YQLORA_UNIFORM_MEMBER(ViewUniforms, exposure),
    },
};

inline const UniformLayout kAtmosphereBlock{
    "Atmosphere",
    {kFrameSet, 1},
    sizeof(AtmosphereUniforms),
    {
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, rayleighScattering),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, bottomRadius),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, mieScattering),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, topRadius),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, mieExtinction),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, mieAnisotropy),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, ozoneAbsorption),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, rayleighDensityScale),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, groundAlbedo),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, mieDensityScale),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, enabled),
        YQLORA_UNIFORM_MEMBER(AtmosphereUniforms, aerialPerspective),
    },
};

// Таблицы атмосферы (AtmospherePass), которые читают шейдеры: пропускание
// на кадр, вид неба и воздушная перспектива на вид.
inline constexpr rhi::BindingSlot kTransmittanceLutSlot{kFrameSet, 2};
inline constexpr rhi::BindingSlot kSkyViewLutSlot{kViewSet, 1};
inline constexpr rhi::BindingSlot kAerialPerspectiveLutSlot{kViewSet, 2};

inline const UniformLayout kObjectBlock{
    "Object",
    {kObjectSet, 0},
    sizeof(ObjectUniforms),
    {
        YQLORA_UNIFORM_MEMBER(ObjectUniforms, model),
        YQLORA_UNIFORM_MEMBER(ObjectUniforms, normalMatrix),
    },
};

// Данные блока Frame из снимка кадра.
YQLORA_API FrameUniforms frameUniforms(const FrameData& frame);

// Данные блока Atmosphere из настроек атмосферы окружения.
YQLORA_API AtmosphereUniforms atmosphereUniforms(const Atmosphere& atmosphere);

// Все блоки движка: их сверяют все шейдеры.
inline const UniformLayout* const kEngineBlocks[] = {&kFrameBlock, &kAtmosphereBlock, &kViewBlock, &kObjectBlock};

} // namespace yqlora
