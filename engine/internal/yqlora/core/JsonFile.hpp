#pragma once

#include "yqlora/platform/Files.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>

namespace yqlora {

// Чтение и запись JSON-файлов движка и редактора (настройки, проекты, сцены,
// .meta): одна обработка ошибок и один вид файла. Только заголовок: типы
// nlohmann наружу из библиотеки движка не выходят.

inline constexpr int kJsonIndent = 4;

// nullopt, если файла нет или в нём не JSON.
inline std::optional<nlohmann::json> readJsonFile(const std::filesystem::path& file)
{
    const auto text = platform::readFile(file);
    if (!text) {
        return std::nullopt;
    }
    nlohmann::json json = nlohmann::json::parse(*text, nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded()) {
        return std::nullopt;
    }

    return json;
}

// Атомарно, см. platform::writeFileAtomically.
inline bool writeJsonFile(const std::filesystem::path& file, const nlohmann::json& json)
{
    return platform::writeFileAtomically(file, json.dump(kJsonIndent) + '\n');
}

} // namespace yqlora
