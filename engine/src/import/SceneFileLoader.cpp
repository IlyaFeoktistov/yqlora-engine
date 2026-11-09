#include "yqlora/import/SceneFileLoader.hpp"

#include "yqlora/core/JsonFile.hpp"
#include "yqlora/scene/SceneBranches.hpp"

#include <cassert>

namespace yqlora {

SceneFileLoader::SceneFileLoader(std::string_view extension, std::string_view kind)
    : m_extensions{extension}
    , m_kind(kind)
{
    assert((kind == kinds::kScene || kind == kinds::kPrefab) && "SceneFileLoader: a scene or a prefab");
}

std::span<const std::string_view> SceneFileLoader::extensions() const
{
    return m_extensions;
}

std::string_view SceneFileLoader::kind() const
{
    return m_kind;
}

LoadResult SceneFileLoader::load(const LoadRequest& request) const
{
    auto json = readJsonFile(request.file);
    if (!json) {
        return loadError("the file is not readable JSON");
    }
    auto root = SceneBranches::rootOfFile(std::move(*json));
    if (!root) {
        return loadError("the file is not a scene of a supported version");
    }
    LoadResult result;
    if (m_kind == kinds::kScene) {
        SceneData data;
        data.root = std::move(*root);
        result.resources.push_back(makeLoadedResource({}, std::move(data)));
    } else {
        PrefabData data;
        data.root = std::move(*root);
        result.resources.push_back(makeLoadedResource({}, std::move(data)));
    }

    return result;
}

} // namespace yqlora
