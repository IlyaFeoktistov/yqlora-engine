#include "yqlora/physics/BoxCollider.hpp"

#include <glm/common.hpp>

namespace yqlora {

void BoxCollider::setSize(const Vec3& size)
{
    m_size = glm::max(size, Vec3(kMinColliderSize));
    changed();
}

ColliderShape BoxCollider::shape() const
{
    return BoxShape{m_size * 0.5f};
}

} // namespace yqlora
