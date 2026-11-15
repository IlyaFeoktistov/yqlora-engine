#include "yqlora/physics/CollisionIgnores.hpp"

#include <algorithm>

namespace yqlora {

std::pair<ComponentId, ComponentId> CollisionIgnores::ordered(ComponentId first, ComponentId second)
{
    return {std::min(first, second), std::max(first, second)};
}

bool CollisionIgnores::set(ComponentId first, ComponentId second, bool ignore)
{
    const auto pair = ordered(first, second);

    return ignore ? m_pairs.insert(pair).second : m_pairs.erase(pair) > 0;
}

bool CollisionIgnores::ignored(ComponentId first, ComponentId second) const
{
    return m_pairs.contains(ordered(first, second));
}

} // namespace yqlora
