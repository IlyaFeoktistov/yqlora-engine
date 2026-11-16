#include "yqlora/physics/Joint.hpp"

#include "yqlora/physics/PhysicsWorldAccess.hpp"

#include <algorithm>

namespace yqlora {

namespace {

float halfCone(float degrees)
{
    return std::clamp(degrees, 0.0f, kMaxJointHalfAngle);
}

float twist(float degrees)
{
    return std::clamp(degrees, -kMaxJointHalfAngle, kMaxJointHalfAngle);
}

JointSpring nonNegative(const JointSpring& spring)
{
    return {std::max(spring.frequency, 0.0f), std::max(spring.damping, 0.0f)};
}

// Пределы, в которые входит начальное положение (min <= 0 <= max).
JointLimits aroundStart(const JointLimits& limits)
{
    return {limits.enabled, std::min(limits.min, 0.0f), std::max(limits.max, 0.0f)};
}

bool usableAxis(const Vec3& axis)
{
    return glm::length(axis) > math::kEpsilon;
}

} // namespace

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
    if (usableAxis(axis)) {
        m_axis = axis;
        changed();
    }
}

void AxisJoint::setLimits(const JointLimits& limits)
{
    m_limits = aroundStart(limits);
    tuned();
}

void AxisJoint::setLimitSpring(const JointSpring& spring)
{
    m_limitSpring = nonNegative(spring);
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
    m_definition.spring = nonNegative(spring);
    tuned();
}

void DistanceJoint::setConnectedAnchor(const Vec3& anchor)
{
    m_definition.connectedAnchor = anchor;
    changed();
}

void ConeJoint::setAxis(const Vec3& axis)
{
    if (usableAxis(axis)) {
        m_definition.axis = axis;
        changed();
    }
}

void ConeJoint::setHalfConeAngle(float degrees)
{
    m_definition.halfConeAngle = halfCone(degrees);
    tuned();
}

void SwingTwistJoint::setAxis(const Vec3& axis)
{
    if (usableAxis(axis)) {
        m_definition.axis = axis;
        changed();
    }
}

void SwingTwistJoint::setNormalHalfConeAngle(float degrees)
{
    m_definition.normalHalfConeAngle = halfCone(degrees);
    tuned();
}

void SwingTwistJoint::setPlaneHalfConeAngle(float degrees)
{
    m_definition.planeHalfConeAngle = halfCone(degrees);
    tuned();
}

void SwingTwistJoint::setTwistMin(float degrees)
{
    m_definition.twistMin = std::min(twist(degrees), 0.0f);
    tuned();
}

void SwingTwistJoint::setTwistMax(float degrees)
{
    m_definition.twistMax = std::max(twist(degrees), 0.0f);
    tuned();
}

void SwingTwistJoint::setMaxFrictionTorque(float torque)
{
    m_definition.maxFrictionTorque = std::max(torque, 0.0f);
    tuned();
}

void SwingTwistJoint::setMotor(JointMotorMode mode)
{
    m_definition.motor = mode;
    tuned();
}

void SwingTwistJoint::setMotorTarget(const Vec3& target)
{
    m_definition.motorTarget = target;
    tuned();
}

void SwingTwistJoint::setMaxMotorTorque(float torque)
{
    m_definition.maxMotorTorque = std::max(torque, 0.0f);
    tuned();
}

void SixDofJoint::setLockLinear(const AxisLocks& locks)
{
    m_definition.lockLinear = locks;
    tuned();
}

void SixDofJoint::setLockAngular(const AxisLocks& locks)
{
    m_definition.lockAngular = locks;
    tuned();
}

void SixDofJoint::setLinearX(const JointLimits& limits)
{
    m_definition.linearX = aroundStart(limits);
    tuned();
}

void SixDofJoint::setLinearY(const JointLimits& limits)
{
    m_definition.linearY = aroundStart(limits);
    tuned();
}

void SixDofJoint::setLinearZ(const JointLimits& limits)
{
    m_definition.linearZ = aroundStart(limits);
    tuned();
}

void SixDofJoint::setAngularX(const JointLimits& limits)
{
    m_definition.angularX = aroundStart({limits.enabled, twist(limits.min), twist(limits.max)});
    tuned();
}

void SixDofJoint::setAngularY(const JointLimits& limits)
{
    m_definition.angularY = aroundStart({limits.enabled, twist(limits.min), twist(limits.max)});
    tuned();
}

void SixDofJoint::setAngularZ(const JointLimits& limits)
{
    m_definition.angularZ = aroundStart({limits.enabled, twist(limits.min), twist(limits.max)});
    tuned();
}

void SixDofJoint::setLinearSpring(const JointSpring& spring)
{
    m_definition.linearSpring = nonNegative(spring);
    tuned();
}

} // namespace yqlora
