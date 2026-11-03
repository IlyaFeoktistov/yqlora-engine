#pragma once

#include "yqlora/core/Export.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace yqlora::platform {

// Весь файл байтами; nullopt, если его нет или он не читается.
YQLORA_API std::optional<std::string> readFile(const std::filesystem::path& file);

// Записывает файл целиком через временный файл рядом и переименование: при
// сбое на середине (падение, нет места) прежний файл остаётся целым, а
// временный удаляется. Папку создаёт, если её нет. false, если не вышло.
YQLORA_API bool writeFileAtomically(const std::filesystem::path& file, std::string_view bytes);

} // namespace yqlora::platform
