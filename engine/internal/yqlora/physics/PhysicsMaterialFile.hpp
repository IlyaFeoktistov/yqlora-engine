#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/physics/PhysicsMaterialData.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace yqlora {

// Расширение файла физического материала (.ymat занят материалами рендера).
inline constexpr std::string_view kPhysicsMaterialFileExtension = ".yphysmat";

// Физический материал из документа .yphysmat (формат "PhysicsMaterial"
// версии 1): поля записи PhysicsMaterialData, недостающие - по умолчанию,
// числа вне пределов прижимаются к ним. nullopt и текст в error, если
// документ не материал, его версия не читается или поле не того типа.
YQLORA_API std::optional<PhysicsMaterialData> physicsMaterialFromJson(
    const nlohmann::json& document,
    std::string& error
);

YQLORA_API nlohmann::json physicsMaterialToJson(const PhysicsMaterialData& material);

// Атомарно (platform::writeFileAtomically).
YQLORA_API bool writePhysicsMaterialFile(const std::filesystem::path& file, const PhysicsMaterialData& material);

} // namespace yqlora
