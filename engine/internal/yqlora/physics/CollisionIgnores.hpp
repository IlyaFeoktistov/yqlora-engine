#pragma once

#include "yqlora/scene/NodeId.hpp"
#include "yqlora/scene/ObjectId.hpp"

#include <map>
#include <set>
#include <utility>

namespace yqlora {

// Пары коллайдеров, которые не сталкиваются, хотя их слои сталкиваются:
// оружие и рука персонажа, снаряд и тот, кто его выпустил; и пары тел
// (узлов-хозяев), связанных скреплением без collideConnected. Пара не
// зависит от порядка. Меняется только в главном потоке между шагами, во
// время шага её читают потоки Jolt (JoltContacts::OnContactValidate).
class CollisionIgnores {
public:
    // true, если что-то изменилось.
    bool set(ComponentId first, ComponentId second, bool ignore);

    bool ignored(ComponentId first, ComponentId second) const;

    // Тела узлов first и second не сталкиваются, пока пару держит хоть одно
    // скрепление: каждое добавление снимается своим удалением.
    void addBodyPair(NodeId first, NodeId second);
    void removeBodyPair(NodeId first, NodeId second);
    bool bodiesIgnored(NodeId first, NodeId second) const;

    bool empty() const
    {
        return m_pairs.empty() && m_bodyPairs.empty();
    }

private:
    template <typename Id>
    static std::pair<Id, Id> ordered(Id first, Id second);

    std::set<std::pair<ComponentId, ComponentId>> m_pairs;
    std::map<std::pair<NodeId, NodeId>, int> m_bodyPairs;
};

} // namespace yqlora
