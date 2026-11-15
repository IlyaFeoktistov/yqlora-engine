#pragma once

#include "yqlora/scene/ObjectId.hpp"

#include <set>
#include <utility>

namespace yqlora {

// Пары коллайдеров, которые не сталкиваются, хотя их слои сталкиваются:
// оружие и рука персонажа, снаряд и тот, кто его выпустил. Пара не зависит
// от порядка. Меняется только в главном потоке между шагами, во время шага
// её читают потоки Jolt (JoltContacts::OnContactValidate).
class CollisionIgnores {
public:
    // true, если что-то изменилось.
    bool set(ComponentId first, ComponentId second, bool ignore);

    bool ignored(ComponentId first, ComponentId second) const;

    bool empty() const
    {
        return m_pairs.empty();
    }

private:
    static std::pair<ComponentId, ComponentId> ordered(ComponentId first, ComponentId second);

    std::set<std::pair<ComponentId, ComponentId>> m_pairs;
};

} // namespace yqlora
