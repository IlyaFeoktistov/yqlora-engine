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

void Collider::setMaterial(Ref<PhysicsMaterialData> material)
{
    m_material = material;
    changed();
}

void Collider::setTrigger(bool trigger)
{
    m_isTrigger = trigger;
    changed();
}

void Collider::setDetectStatic(bool detect)
{
    m_detectStatic = detect;
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
