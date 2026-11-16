#include "yqlora/physics/Joint.hpp"

#include "yqlora/physics/PhysicsWorldAccess.hpp"

#include <algorithm>

namespace yqlora {

void Joint::setConnectedBody(const NodeRef& body)
{
    m_connectedBody = body;
    changed();
}

void Joint::setAnchor(const Vec3& anchor)
{
    m_anchor = anchor;
    changed();
}

void Joint::setCollideConnected(bool collide)
{
    m_collideConnected = collide;
    changed();
}

void Joint::setBreakForce(float force)
{
    m_breakForce = std::max(force, 0.0f);
}

void Joint::setBreakTorque(float torque)
{
    m_breakTorque = std::max(torque, 0.0f);
}

void Joint::setSolverStepsOverride(int steps)
{
    m_solverStepsOverride = std::clamp(steps, 0, kMaxJointSolverSteps);
    tuned();
}

void Joint::changed()
{
    PhysicsWorldAccess::changed(*this);
}

void Joint::tuned()
{
    PhysicsWorldAccess::tuned(*this);
}

void Joint::onEnterScene()
{
    PhysicsWorldAccess::changed(*this);
}

void Joint::onExitScene()
{
    PhysicsWorldAccess::left(*this);
}

void AxisJoint::setAxis(const Vec3& axis)
{
    if (glm::length(axis) > math::kEpsilon) {
        m_axis = axis;
        changed();
    }
}

void AxisJoint::setLimits(const JointLimits& limits)
{
    m_limits = limits;
    // Предел включает начальное положение: min не больше нуля, max не меньше.
    m_limits.min = std::min(m_limits.min, 0.0f);
    m_limits.max = std::max(m_limits.max, 0.0f);
    tuned();
}

void AxisJoint::setLimitSpring(const JointSpring& spring)
{
    m_limitSpring = {std::max(spring.frequency, 0.0f), std::max(spring.damping, 0.0f)};
    tuned();
}

void AxisJoint::setMotor(JointMotorMode mode)
{
    m_motor.mode = mode;
    tuned();
}

void AxisJoint::setMotorTarget(float target)
{
    m_motor.target = target;
    tuned();
}

void AxisJoint::setMaxMotorForce(float force)
{
    m_motor.maxForce = std::max(force, 0.0f);
    tuned();
}

JointDefinition HingeJoint::definition() const
{
    return HingeJointDefinition{axis(), limits(), limitSpring(), motorSettings()};
}

JointDefinition SliderJoint::definition() const
{
    return SliderJointDefinition{axis(), limits(), limitSpring(), motorSettings()};
}

void DistanceJoint::setMinDistance(float distance)
{
    m_definition.minDistance = distance;
    tuned();
}

void DistanceJoint::setMaxDistance(float distance)
{
    m_definition.maxDistance = distance;
    tuned();
}

void DistanceJoint::setSpring(const JointSpring& spring)
{
    m_definition.spring = {std::max(spring.frequency, 0.0f), std::max(spring.damping, 0.0f)};
    tuned();
}

void DistanceJoint::setConnectedAnchor(const Vec3& anchor)
{
    m_definition.connectedAnchor = anchor;
    changed();
}

} // namespace yqlora
