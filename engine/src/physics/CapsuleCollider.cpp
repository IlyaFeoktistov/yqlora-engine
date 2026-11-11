#include "yqlora/physics/CapsuleCollider.hpp"

#include <algorithm>

namespace yqlora {

void CapsuleCollider::setRadius(float radius)
{
    m_radius = std::max(radius, kMinColliderSize);
    changed();
}

void CapsuleCollider::setHeight(float height)
{
    m_height = std::max(height, kMinColliderSize);
    changed();
}

ColliderShape CapsuleCollider::shape() const
{
    return CapsuleShape{m_radius, std::max(m_height * 0.5f - m_radius, 0.0f)};
}

} // namespace yqlora
