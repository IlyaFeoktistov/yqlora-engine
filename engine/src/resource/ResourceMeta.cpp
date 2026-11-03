#include "yqlora/resource/ResourceMeta.hpp"

#include "yqlora/core/FileFormat.hpp"
#include "yqlora/core/JsonFile.hpp"
#include "yqlora/core/JsonId.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"

#include <array>

namespace yqlora {

namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr const char* kIdKey = "id";
constexpr const char* kImportKey = "import";
constexpr const char* kSubresourcesKey = "subresources";

constexpr int kFirstMetaVersion = 1;
constexpr std::array<Migration, 0> kMetaMigrations{};
constexpr FileFormat kMetaFormat{"Resource meta", kFirstMetaVersion, kMetaMigrations};

} // namespace

fs::path metaPathOf(const fs::path& file)
{
    fs::path result = file;
    result += ResourceDatabase::metaExtension();

    return result;
}

bool isMetaFile(const fs::path& path)
{
    return path.extension() == ResourceDatabase::metaExtension();
}

std::optional<ResourceMeta> readResourceMeta(const fs::path& file)
{
    std::error_code ec;
    if (!fs::exists(metaPathOf(file), ec)) {
        return std::nullopt;
    }
    const auto json = readJsonFile(metaPathOf(file));
    const bool current = json && json->is_object()
                         && kMetaFormat.compatibility(FileFormat::versionOf(*json)) == FormatCompatibility::Current;
    const auto id = current && json->contains(kIdKey) ? idFromJson((*json)[kIdKey]) : std::nullopt;
    if (!id) {
        log::warn("Resource meta of '{}' is broken, the file gets a new id", toUtf8(file));

        return std::nullopt;
    }
    ResourceMeta meta{.id = *id};
    if (const auto import = json->find(kImportKey); import != json->end() && import->is_object()) {
        meta.import = import->dump();
    }
    if (const auto nested = json->find(kSubresourcesKey); nested != json->end() && nested->is_object()) {
        for (const auto& [key, value] : nested->items()) {
            meta.subresources[key] = idFromJson(value).value_or(0);
        }
    }

    return meta;
}

bool writeResourceMeta(const fs::path& file, const ResourceMeta& meta)
{
    Json json;
    kMetaFormat.stamp(json);
    json[kIdKey] = idToJson(meta.id);
    if (meta.import != kNoImportSettings) {
        json[kImportKey] = Json::parse(meta.import);
    }
    if (!meta.subresources.empty()) {
        Json nested = Json::object();
        for (const auto& [key, value] : meta.subresources) {
            nested[key] = idToJson(value);
        }
        json[kSubresourcesKey] = std::move(nested);
    }

    return writeJsonFile(metaPathOf(file), json);
}

} // namespace yqlora
