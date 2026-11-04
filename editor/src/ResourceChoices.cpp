#include "yqlora/editor/ResourceChoices.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/ProjectFiles.hpp"
#include "yqlora/editor/ResourceLabel.hpp"
#include "yqlora/render/ResourceUses.hpp"
#include "yqlora/resource/BuiltinResourceTable.hpp"
#include "yqlora/resource/BuiltinResources.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"
#include "yqlora/resource/ResourceLoader.hpp"
#include "yqlora/scene/SceneSerializer.hpp"

#include <algorithm>

namespace yqlora::editor {

namespace {

std::vector<ResourceId> nestedOfKind(
    std::string_view kind,
    std::string_view use,
    ResourceId file,
    const std::filesystem::path& path,
    ResourceCache& cache
)
{
    std::vector<ResourceId> nested;
    const auto kinds = cache.nestedKindsOf(path);
    if (std::ranges::find(kinds, kind) == kinds.end()) {
        return nested;
    }
    for (const ResourceId id : nestedResources(file, path, cache)) {
        if (cache.loadedKind(id) == kind && fitsResourceUse(use, id, cache)) {
            nested.push_back(id);
        }
    }

    return nested;
}

} // namespace

std::string_view fileKind(const std::filesystem::path& path, const ResourceCache& cache)
{
    return ProjectFiles::isScene(path) ? SceneSerializer::kResourceKind : cache.kindOf(path);
}

std::vector<ResourceId> nestedResources(ResourceId file, const std::filesystem::path& path, ResourceCache& cache)
{
    std::vector<ResourceId> nested;
    if (cache.nestedKindsOf(path).empty()) {
        return nested;
    }
    for (const ResourceId id : cache.resourcesOf(file)) {
        if (id != file && !cache.loadedKind(id).empty()) {
            nested.push_back(id);
        }
    }

    return nested;
}

std::vector<ResourceChoice> resourceChoices(
    std::string_view kind,
    std::string_view use,
    const ResourceDatabase* resources,
    ResourceCache& cache
)
{
    std::vector<ResourceChoice> choices;
    for (const BuiltinResourceEntry& entry : builtinResourceTable()) {
        if (entry.kind == kind && fitsResourceUse(use, entry.id, cache)) {
            choices.push_back({entry.id, std::string(entry.name), {}, true});
        }
    }
    if (!resources) {
        return choices;
    }
    for (const auto& [file, path] : resources->paths()) {
        if (fileKind(path, cache) == kind && fitsResourceUse(use, file, cache)) {
            choices.push_back({file, toGenericUtf8(path), {}, false});
        }
        for (const ResourceId id : nestedOfKind(kind, use, file, path, cache)) {
            choices.push_back({id, std::string(subresourceLocalKey(resources->key(id))), toGenericUtf8(path), false});
        }
    }

    return choices;
}

std::optional<ResourceId> resourceForField(
    std::string_view kind,
    std::string_view use,
    ResourceId id,
    const ResourceDatabase* resources,
    ResourceCache& cache
)
{
    if (builtin::isBuiltin(id)) {
        const BuiltinResourceEntry* entry = findBuiltinResource(id);

        return entry && entry->kind == kind && fitsResourceUse(use, id, cache) ? std::optional(id) : std::nullopt;
    }
    const auto path = resources ? resources->path(id) : std::nullopt;
    if (!path) {
        return std::nullopt;
    }
    if (!resources->key(id).empty()) {
        return cache.loadedKind(id) == kind && fitsResourceUse(use, id, cache) ? std::optional(id) : std::nullopt;
    }
    if (fileKind(*path, cache) == kind) {
        return fitsResourceUse(use, id, cache) ? std::optional(id) : std::nullopt;
    }
    const auto nested = nestedOfKind(kind, use, id, *path, cache);

    return nested.empty() ? std::nullopt : std::optional(nested.front());
}

} // namespace yqlora::editor
