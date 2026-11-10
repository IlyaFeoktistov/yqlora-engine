#pragma once

#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <nlohmann/json.hpp>

#include <memory>

namespace yqlora {

// Корень экземпляра префаба (Node::prefabInstance): из чего экземпляр
// развёрнут и что в его записи не нашло цели.
struct PrefabInstance {
    ResourceId prefab = 0;
    NodeId id = 0;
    // Дерево префаба в пространстве префаба, вложенные префабы развёрнуты
    // (PrefabExpansion.hpp): с ним сравнивается экземпляр, когда считаются
    // переопределения. Держится здесь, поэтому перезагрузка префаба его не
    // освобождает. nullptr - префаб не развернулся (нет файла, цикл).
    std::shared_ptr<const nlohmann::json> base;
    // Переопределения без цели в формате записи; если base нет - вся запись,
    // и она пишется обратно как была.
    nlohmann::json unused;
};

} // namespace yqlora
