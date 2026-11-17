#include "yqlora/physics/PhysicsTypes.hpp"

#include "yqlora/physics/BoxCollider.hpp"
#include "yqlora/physics/CapsuleCollider.hpp"
#include "yqlora/physics/CharacterController.hpp"
#include "yqlora/physics/ConvexCollider.hpp"
#include "yqlora/physics/CylinderCollider.hpp"
#include "yqlora/physics/Joint.hpp"
#include "yqlora/physics/MeshCollider.hpp"
#include "yqlora/physics/PhysicsChain.hpp"
#include "yqlora/physics/PhysicsSettings.hpp"
#include "yqlora/physics/PhysicsWorld.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/physics/SphereCollider.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <vector>

namespace yqlora {

void registerPhysicsTypes(TypeRegistry& registry)
{
    const PropertyValue kForceDefault = PropertyStorage<ForceMode>::store(ForceMode::Force);
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
        .property("interpolation", &RigidBody::interpolation, &RigidBody::setInterpolation)
        .property("continuousCollision", &RigidBody::continuousCollision, &RigidBody::setContinuousCollision)
        .property("lockPosition", &RigidBody::lockPosition, &RigidBody::setLockPosition)
        .property("lockRotation", &RigidBody::lockRotation, &RigidBody::setLockRotation)
        .property("startAsleep", &RigidBody::startAsleep, &RigidBody::setStartAsleep)
        .method("velocity", &RigidBody::velocity)
        .method("setVelocity", &RigidBody::setVelocity, {{"velocity"}})
        .method("angularVelocity", &RigidBody::angularVelocity)
        .method("setAngularVelocity", &RigidBody::setAngularVelocity, {{"velocity"}})
        .method("addForce", &RigidBody::addForce, {{"force"}, {"mode", kForceDefault}})
        .method("addForceAtPoint", &RigidBody::addForceAtPoint, {{"force"}, {"point"}, {"mode", kForceDefault}})
        .method("addTorque", &RigidBody::addTorque, {{"torque"}, {"mode", kForceDefault}})
        .method("moveTo", &RigidBody::moveTo, {{"position"}})
        .method("sleep", &RigidBody::sleep)
        .method("wakeUp", &RigidBody::wakeUp)
        .method("isSleeping", &RigidBody::isSleeping)
        .event("onCollisionEnter", &RigidBody::onCollisionEnter, {"collision"})
        .event("onCollisionStay", &RigidBody::onCollisionStay, {"collision"})
        .event("onCollisionExit", &RigidBody::onCollisionExit, {"collision"});

    // Несколько коллайдеров на узле складываются в одно тело.
    registry.registerComponent<Collider>()
        .requiresNode<Node3D>()
        .allowMultiple()
        .property("center", &Collider::center, &Collider::setCenter)
        .property("rotation", &Collider::rotationDegrees, &Collider::setRotationDegrees, PropertyHint::Angle)
        .property("material", &Collider::material, &Collider::setMaterial)
        .property("isTrigger", &Collider::isTrigger, &Collider::setTrigger)
        .property("detectStatic", &Collider::detectStatic, &Collider::setDetectStatic)
        .event("onTriggerEnter", &Collider::onTriggerEnter, {"other"})
        .event("onTriggerExit", &Collider::onTriggerExit, {"other"});
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
    registry.registerComponent<ConvexCollider>()
        .property("mesh", &ConvexCollider::mesh, &ConvexCollider::setMesh)
        .resource(MeshData::kResourceKind);
    registry.registerComponent<MeshCollider>()
        .property("mesh", &MeshCollider::mesh, &MeshCollider::setMesh)
        .resource(MeshData::kResourceKind);
    registry.registerComponent<CylinderCollider>()
        .property("radius", &CylinderCollider::radius, &CylinderCollider::setRadius)
        .min(kMinColliderSize)
        .property("height", &CylinderCollider::height, &CylinderCollider::setHeight)
        .min(kMinColliderSize);

    registry.registerComponent<CharacterController>()
        .requiresNode<Node3D>()
        .property("radius", &CharacterController::radius, &CharacterController::setRadius)
        .min(kMinCharacterSize)
        .property("height", &CharacterController::height, &CharacterController::setHeight)
        .min(kMinCharacterSize)
        .property("center", &CharacterController::center, &CharacterController::setCenter)
        .property(
            "slopeLimit",
            &CharacterController::slopeLimit,
            &CharacterController::setSlopeLimit,
            PropertyHint::Angle
        )
        .range(0.0f, kMaxCharacterSlope)
        .property("stepHeight", &CharacterController::stepHeight, &CharacterController::setStepHeight)
        .min(0.0f)
        .property("walkStairs", &CharacterController::walkStairs, &CharacterController::setWalkStairs)
        .property("stickToGround", &CharacterController::stickToGround, &CharacterController::setStickToGround)
        .property("stickDistance", &CharacterController::stickDistance, &CharacterController::setStickDistance)
        .min(0.0f)
        .property("slopeSpeed", &CharacterController::slopeSpeed, &CharacterController::setSlopeSpeed)
        .property("skinWidth", &CharacterController::skinWidth, &CharacterController::setSkinWidth)
        .min(0.0f)
        .property("mass", &CharacterController::mass, &CharacterController::setMass)
        .min(kMinCharacterSize)
        .property("pushForce", &CharacterController::pushForce, &CharacterController::setPushForce)
        .min(0.0f)
        .method("move", &CharacterController::move, {{"velocity"}, {"deltaSeconds"}})
        .method("isGrounded", &CharacterController::isGrounded)
        .method("groundNormal", &CharacterController::groundNormal)
        .method("groundVelocity", &CharacterController::groundVelocity)
        .method("velocity", &CharacterController::velocity)
        .method("resize", &CharacterController::resize, {{"height"}})
        .event("onControllerHit", &CharacterController::onControllerHit, {"other"});

    // Звено цепи держит скрепление с предыдущим звеном и со следующим.
    registry.registerComponent<Joint>()
        .requiresComponent<RigidBody>()
        .allowMultiple()
        .property("connectedBody", &Joint::connectedBody, &Joint::setConnectedBody)
        .property("anchor", &Joint::anchor, &Joint::setAnchor)
        .property("collideConnected", &Joint::collideConnected, &Joint::setCollideConnected)
        .property("breakForce", &Joint::breakForce, &Joint::setBreakForce)
        .min(0.0f)
        .property("breakTorque", &Joint::breakTorque, &Joint::setBreakTorque)
        .min(0.0f)
        .property("solverStepsOverride", &Joint::solverStepsOverride, &Joint::setSolverStepsOverride)
        .range(0, kMaxJointSolverSteps)
        .method("force", &Joint::force)
        .method("torque", &Joint::torque)
        .event("onBreak", &Joint::onBreak);
    registry.registerComponent<AxisJoint>()
        .property("axis", &AxisJoint::axis, &AxisJoint::setAxis)
        .property("limits", &AxisJoint::limits, &AxisJoint::setLimits)
        .property("limitSpring", &AxisJoint::limitSpring, &AxisJoint::setLimitSpring)
        .property("motor", &AxisJoint::motor, &AxisJoint::setMotor)
        .property("motorTarget", &AxisJoint::motorTarget, &AxisJoint::setMotorTarget)
        .shownWhen("motor", {"Velocity", "Position"})
        .property("maxMotorForce", &AxisJoint::maxMotorForce, &AxisJoint::setMaxMotorForce)
        .min(0.0f)
        .shownWhen("motor", {"Velocity", "Position"});
    registry.registerComponent<FixedJoint>();
    registry.registerComponent<PointJoint>();
    registry.registerComponent<HingeJoint>().method("angle", &HingeJoint::angle);
    registry.registerComponent<SliderJoint>().method("position", &SliderJoint::position);
    registry.registerComponent<DistanceJoint>()
        .property("connectedAnchor", &DistanceJoint::connectedAnchor, &DistanceJoint::setConnectedAnchor)
        .property("minDistance", &DistanceJoint::minDistance, &DistanceJoint::setMinDistance)
        .property("maxDistance", &DistanceJoint::maxDistance, &DistanceJoint::setMaxDistance)
        .property("spring", &DistanceJoint::spring, &DistanceJoint::setSpring);
    registry.registerComponent<ConeJoint>()
        .property("axis", &ConeJoint::axis, &ConeJoint::setAxis)
        .property("halfConeAngle", &ConeJoint::halfConeAngle, &ConeJoint::setHalfConeAngle, PropertyHint::Angle)
        .range(0.0f, kMaxJointHalfAngle);
    registry.registerComponent<SwingTwistJoint>()
        .property("axis", &SwingTwistJoint::axis, &SwingTwistJoint::setAxis)
        .property(
            "normalHalfConeAngle",
            &SwingTwistJoint::normalHalfConeAngle,
            &SwingTwistJoint::setNormalHalfConeAngle,
            PropertyHint::Angle
        )
        .range(0.0f, kMaxJointHalfAngle)
        .property(
            "planeHalfConeAngle",
            &SwingTwistJoint::planeHalfConeAngle,
            &SwingTwistJoint::setPlaneHalfConeAngle,
            PropertyHint::Angle
        )
        .range(0.0f, kMaxJointHalfAngle)
        .property("twistMin", &SwingTwistJoint::twistMin, &SwingTwistJoint::setTwistMin, PropertyHint::Angle)
        .range(-kMaxJointHalfAngle, 0.0f)
        .property("twistMax", &SwingTwistJoint::twistMax, &SwingTwistJoint::setTwistMax, PropertyHint::Angle)
        .range(0.0f, kMaxJointHalfAngle)
        .property("maxFrictionTorque", &SwingTwistJoint::maxFrictionTorque, &SwingTwistJoint::setMaxFrictionTorque)
        .min(0.0f)
        .property("motor", &SwingTwistJoint::motor, &SwingTwistJoint::setMotor)
        .property("motorTarget", &SwingTwistJoint::motorTarget, &SwingTwistJoint::setMotorTarget)
        .shownWhen("motor", {"Velocity", "Position"})
        .property("maxMotorTorque", &SwingTwistJoint::maxMotorTorque, &SwingTwistJoint::setMaxMotorTorque)
        .min(0.0f)
        .shownWhen("motor", {"Velocity", "Position"});
    registry.registerComponent<SixDofJoint>()
        .property("lockLinear", &SixDofJoint::lockLinear, &SixDofJoint::setLockLinear)
        .property("lockAngular", &SixDofJoint::lockAngular, &SixDofJoint::setLockAngular)
        .property("linearX", &SixDofJoint::linearX, &SixDofJoint::setLinearX)
        .property("linearY", &SixDofJoint::linearY, &SixDofJoint::setLinearY)
        .property("linearZ", &SixDofJoint::linearZ, &SixDofJoint::setLinearZ)
        .property("angularX", &SixDofJoint::angularX, &SixDofJoint::setAngularX)
        .property("angularY", &SixDofJoint::angularY, &SixDofJoint::setAngularY)
        .property("angularZ", &SixDofJoint::angularZ, &SixDofJoint::setAngularZ)
        .property("linearSpring", &SixDofJoint::linearSpring, &SixDofJoint::setLinearSpring);

    registry.registerComponent<PhysicsChain>()
        .requiresNode<Node3D>()
        .property("segments", &PhysicsChain::segments, &PhysicsChain::setSegments)
        .range(kMinChainSegments, kMaxChainSegments)
        .property("length", &PhysicsChain::length, &PhysicsChain::setLength)
        .min(kMinChainSize)
        .property("radius", &PhysicsChain::radius, &PhysicsChain::setRadius)
        .min(kMinChainSize)
        .property("mass", &PhysicsChain::mass, &PhysicsChain::setMass)
        .min(kMinChainSize)
        .property("bendLimit", &PhysicsChain::bendLimit, &PhysicsChain::setBendLimit, PropertyHint::Angle)
        .range(0.0f, kMaxChainBendDegrees)
        .property("twistLimit", &PhysicsChain::twistLimit, &PhysicsChain::setTwistLimit, PropertyHint::Angle)
        .range(0.0f, kMaxChainBendDegrees)
        .property("stiffness", &PhysicsChain::stiffness, &PhysicsChain::setStiffness)
        .min(0.0f)
        .property("damping", &PhysicsChain::damping, &PhysicsChain::setDamping)
        .min(0.0f)
        .property("maxTorque", &PhysicsChain::maxTorque, &PhysicsChain::setMaxTorque)
        .min(0.0f)
        .property("sideGrip", &PhysicsChain::sideGrip, &PhysicsChain::setSideGrip)
        .range(0.0f, 1.0f)
        .property("forwardGrip", &PhysicsChain::forwardGrip, &PhysicsChain::setForwardGrip)
        .range(0.0f, 1.0f)
        .property("material", &PhysicsChain::material, &PhysicsChain::setMaterial)
        .method("setBend", &PhysicsChain::setBend, {{"index"}, {"side"}, {"up"}})
        .method("setBends", &PhysicsChain::setBends, {{"bends"}})
        .method("targetBend", &PhysicsChain::targetBend, {{"index"}})
        .method("bend", &PhysicsChain::bend, {{"index"}})
        .method("linkPosition", &PhysicsChain::linkPosition, {{"index"}})
        .method("linkVelocity", &PhysicsChain::linkVelocity, {{"index"}})
        .method("relax", &PhysicsChain::relax)
        .method("relaxed", &PhysicsChain::relaxed)
        .event("onCollisionEnter", &PhysicsChain::onCollisionEnter, {"collision"})
        .event("onCollisionStay", &PhysicsChain::onCollisionStay, {"collision"})
        .event("onCollisionExit", &PhysicsChain::onCollisionExit, {"collision"});

    registry.registerSettings<PhysicsSettings>("physics")
        .property("gravity", &PhysicsSettings::gravity, &PhysicsSettings::setGravity)
        .property("velocitySteps", &PhysicsSettings::velocitySteps, &PhysicsSettings::setVelocitySteps)
        .range(kMinSolverSteps, kMaxSolverSteps)
        .property("positionSteps", &PhysicsSettings::positionSteps, &PhysicsSettings::setPositionSteps)
        .range(kMinSolverSteps, kMaxSolverSteps)
        .property("defaultMaterial", &PhysicsSettings::defaultMaterial, &PhysicsSettings::setDefaultMaterial)
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
