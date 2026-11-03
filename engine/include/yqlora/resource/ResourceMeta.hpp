#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace yqlora {

// Настройки импорта, когда их нет: пустой объект JSON.
inline constexpr std::string_view kNoImportSettings = "{}";

// Содержимое файла <ресурс>.meta (формат "Resource meta" версии 1): ID
// файла, ID его вложенных ресурсов по ключам и настройки импорта. Здесь
// только формат; что с ним делать, решает ResourceDatabase.
struct ResourceMeta {
    ResourceId id = 0;
    std::map<std::string, ResourceId> subresources{};
    // Объект JSON текстом: его читает загрузчик своего вида.
    std::string import = std::string(kNoImportSettings);
};

// <файл>.meta рядом с файлом.
YQLORA_API std::filesystem::path metaPathOf(const std::filesystem::path& file);

YQLORA_API bool isMetaFile(const std::filesystem::path& path);

// .meta ресурса file. nullopt, если её нет или она не читается (тогда в
// логе предупреждение). Битый ID вложенного ресурса читается как 0.
YQLORA_API std::optional<ResourceMeta> readResourceMeta(const std::filesystem::path& file);

// Атомарно (platform::writeFileAtomically). Пустые настройки импорта и
// вложенные ресурсы не пишутся.
YQLORA_API bool writeResourceMeta(const std::filesystem::path& file, const ResourceMeta& meta);

} // namespace yqlora
