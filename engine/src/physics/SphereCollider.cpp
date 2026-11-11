#include "yqlora/physics/SphereCollider.hpp"

#include <algorithm>

namespace yqlora {

void SphereCollider::setRadius(float radius)
{
    m_radius = std::max(radius, kMinColliderSize);
    changed();
}

ColliderShape SphereCollider::shape() const
{
    return SphereShape{m_radius};
}

} // namespace yqlora
