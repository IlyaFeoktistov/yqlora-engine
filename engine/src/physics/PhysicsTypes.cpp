#include "yqlora/physics/PhysicsTypes.hpp"

#include "yqlora/physics/BoxCollider.hpp"
#include "yqlora/physics/CapsuleCollider.hpp"
#include "yqlora/physics/CylinderCollider.hpp"
#include "yqlora/physics/PhysicsSettings.hpp"
#include "yqlora/physics/PhysicsWorld.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/physics/SphereCollider.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <vector>

namespace yqlora {

void registerPhysicsTypes(TypeRegistry& registry)
{
    registry.registerSystem<PhysicsWorld>();

    registry.registerComponent<RigidBody>()
        .requiresNode<Node3D>()
        .property("motion", &RigidBody::motion, &RigidBody::setMotion)
        .property("mass", &RigidBody::mass, &RigidBody::setMass)
        .min(kMinBodyMass)
        .property("linearDamping", &RigidBody::linearDamping, &RigidBody::setLinearDamping)
        .min(0.0f)
        .property("angularDamping", &RigidBody::angularDamping, &RigidBody::setAngularDamping)
        .min(0.0f)
        .property("gravityScale", &RigidBody::gravityScale, &RigidBody::setGravityScale)
        .property("interpolation", &RigidBody::interpolation, &RigidBody::setInterpolation);

    // Несколько коллайдеров на узле складываются в одно тело.
    registry.registerComponent<Collider>()
        .requiresNode<Node3D>()
        .allowMultiple()
        .property("center", &Collider::center, &Collider::setCenter)
        .property("rotation", &Collider::rotationDegrees, &Collider::setRotationDegrees, PropertyHint::Angle);
    registry.registerComponent<BoxCollider>()
        .property("size", &BoxCollider::size, &BoxCollider::setSize)
        .min(kMinColliderSize);
    registry.registerComponent<SphereCollider>()
        .property("radius", &SphereCollider::radius, &SphereCollider::setRadius)
        .min(kMinColliderSize);
    registry.registerComponent<CapsuleCollider>()
        .property("radius", &CapsuleCollider::radius, &CapsuleCollider::setRadius)
        .min(kMinColliderSize)
        .property("height", &CapsuleCollider::height, &CapsuleCollider::setHeight)
        .min(kMinColliderSize);
    registry.registerComponent<CylinderCollider>()
        .property("radius", &CylinderCollider::radius, &CylinderCollider::setRadius)
        .min(kMinColliderSize)
        .property("height", &CylinderCollider::height, &CylinderCollider::setHeight)
        .min(kMinColliderSize);

    registry.registerSettings<PhysicsSettings>("physics")
        .property("gravity", &PhysicsSettings::gravity, &PhysicsSettings::setGravity)
        .property("velocitySteps", &PhysicsSettings::velocitySteps, &PhysicsSettings::setVelocitySteps)
        .range(kMinSolverSteps, kMaxSolverSteps)
        .property("positionSteps", &PhysicsSettings::positionSteps, &PhysicsSettings::setPositionSteps)
        .range(kMinSolverSteps, kMaxSolverSteps)
        // В файле - пары слоёв, которые не сталкиваются; в окне - таблица.
        .property(
            "ignoredCollisions",
            [](const PhysicsSettings& settings) {
                return settings.collisions().ignoredPairs();
            },
            [](PhysicsSettings& settings, const std::vector<LayerPair>& pairs) {
                settings.collisions().setIgnoredPairs(pairs);
            },
            PropertyHint::CollisionMatrix
        );
}

} // namespace yqlora
