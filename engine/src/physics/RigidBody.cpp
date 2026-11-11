#include "yqlora/physics/RigidBody.hpp"

#include "yqlora/physics/PhysicsWorldAccess.hpp"

#include <algorithm>

namespace yqlora {

void RigidBody::setMotion(BodyMotion motion)
{
    m_motion = motion;
    changed();
}

void RigidBody::setMass(float mass)
{
    m_mass = std::max(mass, kMinBodyMass);
    changed();
}

void RigidBody::setLinearDamping(float damping)
{
    m_linearDamping = std::max(damping, 0.0f);
    changed();
}

void RigidBody::setAngularDamping(float damping)
{
    m_angularDamping = std::max(damping, 0.0f);
    changed();
}

void RigidBody::setGravityScale(float scale)
{
    m_gravityScale = scale;
    changed();
}

void RigidBody::setInterpolation(bool interpolation)
{
    m_interpolation = interpolation;
    changed();
}

void RigidBody::setContinuousCollision(bool continuous)
{
    m_continuousCollision = continuous;
    changed();
}

void RigidBody::setLockPosition(const AxisLocks& locks)
{
    m_lockPosition = locks;
    changed();
}

void RigidBody::setLockRotation(const AxisLocks& locks)
{
    m_lockRotation = locks;
    changed();
}

void RigidBody::setStartAsleep(bool asleep)
{
    m_startAsleep = asleep;
    changed();
}

void RigidBody::changed()
{
    PhysicsWorldAccess::changed(*this);
}

void RigidBody::onEnterScene()
{
    PhysicsWorldAccess::changed(*this);
}

void RigidBody::onExitScene()
{
    PhysicsWorldAccess::left(*this);
}

} // namespace yqlora
