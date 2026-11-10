#include "yqlora/scene/PrefabBases.hpp"

#include "yqlora/resource/ResourceCache.hpp"

#include <algorithm>

namespace yqlora {

namespace {

std::uint64_t revisionOf(ResourceId id, ResourceCache& cache)
{
    const ResourceStatus* status = cache.status(id);

    return status ? status->revision : 0;
}

} // namespace

std::shared_ptr<const nlohmann::json> PrefabBases::find(
    ResourceId prefab,
    std::uint64_t typesRevision,
    ResourceCache& cache
)
{
    const auto it = m_entries.find(prefab);
    if (it == m_entries.end()) {
        return nullptr;
    }
    const Entry& entry = it->second;
    const bool fresh = entry.typesRevision == typesRevision
                       && std::ranges::all_of(entry.revisions, [&](const auto& file) {
                              return revisionOf(file.first, cache) == file.second;
                          });
    if (!fresh) {
        m_entries.erase(it);

        return nullptr;
    }

    return entry.base;
}

void PrefabBases::store(
    ResourceId prefab,
    std::uint64_t typesRevision,
    std::shared_ptr<const nlohmann::json> base,
    std::span<const ResourceId> files,
    ResourceCache& cache
)
{
    Entry entry{std::move(base), typesRevision, {}};
    for (const ResourceId file : files) {
        entry.revisions.emplace_back(file, revisionOf(file, cache));
    }
    m_entries[prefab] = std::move(entry);
}

} // namespace yqlora
