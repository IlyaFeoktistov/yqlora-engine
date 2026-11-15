// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

// Управление телом из кода: часть RigidBody, которой нужен мир Jolt.

#include "yqlora/physics/BodyPose.hpp"
#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/PhysicsWorldImpl.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>

namespace yqlora {

namespace {

// Тело компонента и интерфейс тел его мира; id невалидный - тела нет.
struct BodyHandle {
    JPH::BodyInterface* bodies = nullptr;
    const JPH::BodyLockInterface* locks = nullptr;
    JPH::BodyID id;

    explicit operator bool() const
    {
        return bodies && !id.IsInvalid();
    }

    bool dynamic() const
    {
        return bodies->GetMotionType(id) == JPH::EMotionType::Dynamic;
    }
};

BodyHandle handleOf(const RigidBody& body)
{
    const JoltWorld world = PhysicsWorldJolt::worldOf(body);
    if (!world || !body.owner()) {
        return {};
    }

    return {
        &world.system->GetBodyInterface(),
        &world.system->GetBodyLockInterface(),
        world.bodies->bodyOf(body.owner()->id()),
    };
}

float massOf(const BodyHandle& body)
{
    const JPH::BodyLockRead lock(*body.locks, body.id);
    const JPH::MotionProperties* motion = lock.Succeeded() ? lock.GetBody().GetMotionPropertiesUnchecked() : nullptr;
    const float inverse = motion ? motion->GetInverseMass() : 0.0f;

    return inverse > 0.0f ? 1.0f / inverse : 0.0f;
}

// Импульс или сила по вектору режима: ускорение и прибавка скорости - на массу.
JPH::Vec3 scaledByMass(const BodyHandle& body, const Vec3& value, ForceMode mode)
{
    const bool perMass = mode == ForceMode::Acceleration || mode == ForceMode::VelocityChange;

    return toJolt(perMass ? value * massOf(body) : value);
}

bool isImpulse(ForceMode mode)
{
    return mode == ForceMode::Impulse || mode == ForceMode::VelocityChange;
}

Node3D* spatialOwner(const RigidBody& body)
{
    return dynamic_cast<Node3D*>(body.owner());
}

} // namespace

Vec3 RigidBody::velocity() const
{
    const BodyHandle body = handleOf(*this);

    return body ? fromJolt(body.bodies->GetLinearVelocity(body.id)) : Vec3(0.0f);
}

void RigidBody::setVelocity(const Vec3& velocity)
{
    const BodyHandle body = handleOf(*this);
    if (body && body.dynamic()) {
        body.bodies->SetLinearVelocity(body.id, toJolt(velocity));
        body.bodies->ActivateBody(body.id);
    }
}

Vec3 RigidBody::angularVelocity() const
{
    const BodyHandle body = handleOf(*this);

    return body ? fromJolt(body.bodies->GetAngularVelocity(body.id)) : Vec3(0.0f);
}

void RigidBody::setAngularVelocity(const Vec3& velocity)
{
    const BodyHandle body = handleOf(*this);
    if (body && body.dynamic()) {
        body.bodies->SetAngularVelocity(body.id, toJolt(velocity));
        body.bodies->ActivateBody(body.id);
    }
}

void RigidBody::addForce(const Vec3& force, ForceMode mode)
{
    const BodyHandle body = handleOf(*this);
    if (!body || !body.dynamic()) {
        return;
    }
    const JPH::Vec3 value = scaledByMass(body, force, mode);
    if (isImpulse(mode)) {
        body.bodies->AddImpulse(body.id, value);
    } else {
        body.bodies->AddForce(body.id, value);
    }
}

void RigidBody::addForceAtPoint(const Vec3& force, const Vec3& point, ForceMode mode)
{
    const BodyHandle body = handleOf(*this);
    if (!body || !body.dynamic()) {
        return;
    }
    const JPH::Vec3 value = scaledByMass(body, force, mode);
    if (isImpulse(mode)) {
        body.bodies->AddImpulse(body.id, value, toJolt(point));
    } else {
        body.bodies->AddForce(body.id, value, toJolt(point), JPH::EActivation::Activate);
    }
}

void RigidBody::addTorque(const Vec3& torque, ForceMode mode)
{
    const BodyHandle body = handleOf(*this);
    if (!body || !body.dynamic()) {
        return;
    }
    JPH::Vec3 value = toJolt(torque);
    // Угловое ускорение и прибавка угловой скорости - на тензор инерции в мире.
    if (mode == ForceMode::Acceleration || mode == ForceMode::VelocityChange) {
        value = body.bodies->GetInverseInertia(body.id).Inversed3x3().Multiply3x3(value);
    }
    if (isImpulse(mode)) {
        body.bodies->AddAngularImpulse(body.id, value);
    } else {
        body.bodies->AddTorque(body.id, value);
    }
}

void RigidBody::moveTo(const Vec3& position)
{
    if (Node3D* node = spatialOwner(*this)) {
        node->setWorldPosition(position);
    }
}

void RigidBody::rotateTo(const Quat& rotation)
{
    if (Node3D* node = spatialOwner(*this)) {
        const BodyPose pose = bodyPoseOf(node->worldMatrix());
        node->setWorldMatrix(worldWithPose(node->worldMatrix(), {pose.position, rotation}));
    }
}

void RigidBody::teleport(const Vec3& position, const Quat& rotation)
{
    Node3D* node = spatialOwner(*this);
    if (!node) {
        return;
    }
    node->setWorldMatrix(worldWithPose(node->worldMatrix(), {position, rotation}));
    if (const JoltWorld world = PhysicsWorldJolt::worldOf(*this)) {
        world.bodies->place(node->id());
    }
}

void RigidBody::sleep()
{
    if (const BodyHandle body = handleOf(*this)) {
        body.bodies->DeactivateBody(body.id);
    }
}

void RigidBody::wakeUp()
{
    const BodyHandle body = handleOf(*this);
    if (body && body.bodies->GetMotionType(body.id) != JPH::EMotionType::Static) {
        body.bodies->ActivateBody(body.id);
    }
}

bool RigidBody::isSleeping() const
{
    const BodyHandle body = handleOf(*this);

    return !body || !body.bodies->IsActive(body.id);
}

} // namespace yqlora
