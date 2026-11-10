#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace yqlora {

class ResourceCache;

// Развёрнутые деревья префабов (PrefabInstance::base), общие для всех
// экземпляров: повторное развёртывание префаба не разбирает и не
// нормализует его дерево заново. Дерево годно, пока не перезагружен ни один
// из его файлов (префаб и вложенные) и не поменялись типы в реестре.
class YQLORA_API PrefabBases {
public:
    // nullptr, если дерева нет или оно устарело.
    std::shared_ptr<const nlohmann::json> find(ResourceId prefab, std::uint64_t typesRevision, ResourceCache& cache);

    // files - префабы, из которых дерево собрано, сам prefab тоже; их
    // ревизии берутся из кэша сейчас.
    void store(
        ResourceId prefab,
        std::uint64_t typesRevision,
        std::shared_ptr<const nlohmann::json> base,
        std::span<const ResourceId> files,
        ResourceCache& cache
    );

private:
    struct Entry {
        std::shared_ptr<const nlohmann::json> base;
        std::uint64_t typesRevision = 0;
        std::vector<std::pair<ResourceId, std::uint64_t>> revisions;
    };

    std::map<ResourceId, Entry> m_entries;
};

} // namespace yqlora
