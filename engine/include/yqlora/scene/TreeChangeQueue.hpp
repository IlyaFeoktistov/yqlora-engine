#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <vector>

namespace yqlora {

class Scene;

// Изменения дерева, отложенные до конца кадра: так ни одна фаза не видит
// наполовину изменённое дерево. Узлы - по ID: ушедший с уничтоженной веткой
// пропускается.
class YQLORA_API TreeChangeQueue {
public:
    void destroy(const Node& node);
    // Пока у компонентов нет своих ID, компонент ищется среди компонентов его
    // узла по адресу: адрес только сравнивается, не разыменовывается.
    void destroy(const Component& component);
    void setParent(const Node& node, const Node& parent, KeepWorld keepWorld);

    // Применяет изменения по порядку, когда кадр уже кончился.
    void apply(Scene& scene);

private:
    struct Change {
        NodeId node = 0;
        // 0 - уничтожить узел (или компонент, если он задан).
        NodeId parent = 0;
        const Component* component = nullptr;
        KeepWorld keepWorld = KeepWorld::No;
    };

    std::vector<Change> m_changes;
};

} // namespace yqlora
