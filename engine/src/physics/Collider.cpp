#include "yqlora/physics/Collider.hpp"

#include "yqlora/physics/PhysicsWorldAccess.hpp"

#include <glm/gtc/quaternion.hpp>

namespace yqlora {

void Collider::setCenter(const Vec3& center)
{
    m_center = center;
    changed();
}

void Collider::setRotationDegrees(const Vec3& degrees)
{
    m_rotationDegrees = degrees;
    changed();
}

Quat Collider::rotation() const
{
    return Quat(glm::radians(m_rotationDegrees));
}

void Collider::changed()
{
    PhysicsWorldAccess::changed(*this);
}

void Collider::onEnterScene()
{
    PhysicsWorldAccess::changed(*this);
}

void Collider::onExitScene()
{
    PhysicsWorldAccess::left(*this);
}

} // namespace yqlora
