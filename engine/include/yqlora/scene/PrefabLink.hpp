#pragma once

#include "yqlora/scene/NodeId.hpp"

namespace yqlora {

// Откуда узел экземпляра префаба: экземпляр и узел префаба, из которого он
// развёрнут (см. docs/architecture/prefabs.md). У остальных узлов пусто.
struct PrefabLink {
    NodeId instance = 0;
    // ID узла в префабе. Узел вложенного префаба - в пространстве внешнего
    // префаба, как его ID в развёрнутом внешнем.
    NodeId source = 0;

    explicit operator bool() const
    {
        return instance != 0;
    }

    bool operator==(const PrefabLink&) const = default;
};

} // namespace yqlora
