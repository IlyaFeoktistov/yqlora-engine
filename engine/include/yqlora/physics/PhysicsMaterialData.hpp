#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/reflection/Record.hpp"
#include "yqlora/resource/ResourceKinds.hpp"

#include <cstdint>

namespace yqlora {

// Как сводятся значения двух материалов в контакте. Порядок - старшинство:
// когда режимы материалов разные, берётся более поздний в списке, поэтому
// итог не зависит от того, какое тело первое.
enum class PhysicsCombine : std::uint8_t {
    Average,
    Minimum,
    Multiply,
    Maximum,
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::PhysicsCombine, "Average", "Minimum", "Multiply", "Maximum")

namespace yqlora {

// Трение и упругость по умолчанию: дерево или камень, ничего не отскакивает.
inline constexpr float kDefaultFriction = 0.6f;
inline constexpr float kDefaultRestitution = 0.0f;
// Упругость 1 - удар без потери скорости; больше - тело набирало бы энергию.
inline constexpr float kMaxRestitution = 1.0f;

// Ресурс физического материала (.yphysmat и встроенный): поверхность
// коллайдера в контактах. Трение - коэффициент Кулона (0 - лёд без трения,
// 1 и больше - резина), упругость - доля скорости удара, с которой тело
// отскакивает.
struct PhysicsMaterialData {
    static constexpr std::string_view kResourceKind = kinds::kPhysicsMaterial;

    float friction = kDefaultFriction;
    float restitution = kDefaultRestitution;
    PhysicsCombine frictionCombine = PhysicsCombine::Average;
    PhysicsCombine restitutionCombine = PhysicsCombine::Average;

    bool operator==(const PhysicsMaterialData&) const = default;
};

// Трение не меньше 0, упругость в [0, kMaxRestitution].
YQLORA_API PhysicsMaterialData sanitized(const PhysicsMaterialData& material);

// Режим пары: старший из двух (см. PhysicsCombine).
YQLORA_API PhysicsCombine combineMode(PhysicsCombine first, PhysicsCombine second);

YQLORA_API float combineValues(float first, float second, PhysicsCombine mode);

YQLORA_API float combinedFriction(const PhysicsMaterialData& first, const PhysicsMaterialData& second);
YQLORA_API float combinedRestitution(const PhysicsMaterialData& first, const PhysicsMaterialData& second);

} // namespace yqlora

YQLORA_RECORD(
    yqlora::PhysicsMaterialData,
    field("friction", &yqlora::PhysicsMaterialData::friction).min(0.0f),
    field("restitution", &yqlora::PhysicsMaterialData::restitution).range(0.0f, yqlora::kMaxRestitution),
    field("frictionCombine", &yqlora::PhysicsMaterialData::frictionCombine),
    field("restitutionCombine", &yqlora::PhysicsMaterialData::restitutionCombine)
)
