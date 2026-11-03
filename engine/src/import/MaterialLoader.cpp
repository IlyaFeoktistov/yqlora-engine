#include "yqlora/import/MaterialLoader.hpp"

#include "yqlora/core/JsonFile.hpp"
#include "yqlora/material/MaterialFile.hpp"

#include <array>

namespace yqlora {

namespace {

constexpr std::array<std::string_view, 1> kExtensions{kMaterialFileExtension};

} // namespace

std::span<const std::string_view> MaterialLoader::extensions() const
{
    return kExtensions;
}

std::string_view MaterialLoader::kind() const
{
    return MaterialData::kResourceKind;
}

LoadResult MaterialLoader::load(const LoadRequest& request) const
{
    const auto json = readJsonFile(request.file);
    if (!json) {
        return loadError("the file is not readable JSON");
    }
    std::string error;
    auto material = materialFromJson(*json, error);
    if (!material) {
        return loadError(std::move(error));
    }
    LoadResult result;
    result.resources.push_back(makeLoadedResource({}, std::move(*material)));

    return result;
}

} // namespace yqlora
