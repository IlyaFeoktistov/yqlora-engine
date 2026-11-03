#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/material/MaterialData.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace yqlora {

// Расширение файла материала.
inline constexpr std::string_view kMaterialFileExtension = ".ymat";

// Материал из документа .ymat (формат "Material" версии 1). nullopt и текст
// в error, если документ не материал или его версия не читается.
// Неизвестное значение blend или cull - ошибка; битые значения полей и ID
// текстур пропускаются.
YQLORA_API std::optional<MaterialData> materialFromJson(const nlohmann::json& document, std::string& error);

YQLORA_API nlohmann::json materialToJson(const MaterialData& material);

// Атомарно (platform::writeFileAtomically).
YQLORA_API bool writeMaterialFile(const std::filesystem::path& file, const MaterialData& material);

} // namespace yqlora
