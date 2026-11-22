#include "yqlora/physics/CylinderCollider.hpp"

#include <algorithm>
#include <variant>

namespace yqlora {

void CylinderCollider::setRadius(float radius)
{
    m_radius = std::max(radius, kMinColliderSize);
    changed();
}

void CylinderCollider::setHeight(float height)
{
    m_height = std::max(height, kMinColliderSize);
    changed();
}

ColliderShape CylinderCollider::shape() const
{
    return CylinderShape{m_radius, m_height * 0.5f};
}

void CylinderCollider::setShape(const ColliderShape& shape)
{
    if (const auto* cylinder = std::get_if<CylinderShape>(&shape)) {
        setRadius(cylinder->radius);
        setHeight(cylinder->halfHeight * 2.0f);
    }
}

} // namespace yqlora
