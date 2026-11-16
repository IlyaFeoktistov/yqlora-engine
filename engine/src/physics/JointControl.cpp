// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

// Чтение скрепления из кода: часть Joint, которой нужен мир Jolt.

#include "yqlora/physics/Joint.hpp"
#include "yqlora/physics/PhysicsWorldImpl.hpp"

#include <glm/trigonometric.hpp>

namespace yqlora {

float Joint::force() const
{
    const PhysicsJoints* joints = PhysicsWorldJolt::jointsOf(*this);

    return joints ? joints->force(*this) : 0.0f;
}

float Joint::torque() const
{
    const PhysicsJoints* joints = PhysicsWorldJolt::jointsOf(*this);

    return joints ? joints->torque(*this) : 0.0f;
}

float HingeJoint::angle() const
{
    const PhysicsJoints* joints = PhysicsWorldJolt::jointsOf(*this);

    return joints ? glm::degrees(joints->hingeAngle(*this)) : 0.0f;
}

float SliderJoint::position() const
{
    const PhysicsJoints* joints = PhysicsWorldJolt::jointsOf(*this);

    return joints ? joints->sliderPosition(*this) : 0.0f;
}

} // namespace yqlora
