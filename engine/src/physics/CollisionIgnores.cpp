#include "yqlora/physics/CollisionIgnores.hpp"

#include <algorithm>

namespace yqlora {

template <typename Id>
std::pair<Id, Id> CollisionIgnores::ordered(Id first, Id second)
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

void CollisionIgnores::addBodyPair(NodeId first, NodeId second)
{
    ++m_bodyPairs[ordered(first, second)];
}

void CollisionIgnores::removeBodyPair(NodeId first, NodeId second)
{
    const auto it = m_bodyPairs.find(ordered(first, second));
    if (it != m_bodyPairs.end() && --it->second == 0) {
        m_bodyPairs.erase(it);
    }
}

bool CollisionIgnores::bodiesIgnored(NodeId first, NodeId second) const
{
    return m_bodyPairs.contains(ordered(first, second));
}

} // namespace yqlora
