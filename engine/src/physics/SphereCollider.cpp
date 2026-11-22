#include "yqlora/physics/SphereCollider.hpp"

#include <algorithm>
#include <variant>

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

void SphereCollider::setShape(const ColliderShape& shape)
{
    if (const auto* sphere = std::get_if<SphereShape>(&shape)) {
        setRadius(sphere->radius);
    }
}

} // namespace yqlora
