#include "yqlora/import/PhysicsMaterialLoader.hpp"

#include "yqlora/core/JsonFile.hpp"
#include "yqlora/physics/PhysicsMaterialFile.hpp"

#include <array>

namespace yqlora {

namespace {

constexpr std::array<std::string_view, 1> kExtensions{kPhysicsMaterialFileExtension};

} // namespace

std::span<const std::string_view> PhysicsMaterialLoader::extensions() const
{
    return kExtensions;
}

std::string_view PhysicsMaterialLoader::kind() const
{
    return PhysicsMaterialData::kResourceKind;
}

LoadResult PhysicsMaterialLoader::load(const LoadRequest& request) const
{
    const auto json = readJsonFile(request.file);
    if (!json) {
        return loadError("the file is not readable JSON");
    }
    std::string error;
    auto material = physicsMaterialFromJson(*json, error);
    if (!material) {
        return loadError(std::move(error));
    }
    LoadResult result;
    result.resources.push_back(makeLoadedResource({}, std::move(*material)));

    return result;
}

} // namespace yqlora
