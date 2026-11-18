// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsJoints.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/physics/BodyPose.hpp"
#include "yqlora/physics/CollisionIgnores.hpp"
#include "yqlora/physics/Joint.hpp"
#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/PhysicsBodies.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/Scene.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <variant>
#include <vector>

namespace yqlora {

namespace {

JointFrame toWorld(const Mat4& world, const JointFrame& local)
{
    const Quat rotation = bodyPoseOf(world).rotation;

    return {
        .point = Vec3(world * Vec4(local.point, 1.0f)),
        .axis = glm::normalize(rotation * local.axis),
        .normal = glm::normalize(rotation * local.normal),
    };
}

JointFrame toLocal(const Mat4& world, const JointFrame& frame)
{
    const Quat inverse = glm::inverse(bodyPoseOf(world).rotation);

    return {
        .point = Vec3(glm::inverse(world) * Vec4(frame.point, 1.0f)),
        .axis = glm::normalize(inverse * frame.axis),
        .normal = glm::normalize(inverse * frame.normal),
    };
}

JPH::SpringSettings springOf(const JointSpring& spring)
{
    return JPH::SpringSettings(JPH::ESpringMode::FrequencyAndDamping, spring.frequency, spring.damping);
}

// Пределы петли в радианах; Jolt держит их в [-pi, 0] и [0, pi], а полный
// круг - это петля без пределов.
std::pair<float, float> hingeLimits(const JointLimits& limits)
{
    if (!limits.enabled) {
        return {-JPH::JPH_PI, JPH::JPH_PI};
    }

    return {
        std::clamp(glm::radians(limits.min), -JPH::JPH_PI, 0.0f),
        std::clamp(glm::radians(limits.max), 0.0f, JPH::JPH_PI),
    };
}

std::pair<float, float> sliderLimits(const JointLimits& limits)
{
    return limits.enabled ? std::pair{limits.min, limits.max} : std::pair{-FLT_MAX, FLT_MAX};
}

// Пределы оси с шестью степенями: запертая сведена в ноль, без пределов -
// свободна.
std::pair<float, float> axisLimits(bool locked, const JointLimits& limits, float scale)
{
    if (locked) {
        return {FLT_MAX, -FLT_MAX};
    }
    if (!limits.enabled) {
        return {-FLT_MAX, FLT_MAX};
    }

    return {limits.min * scale, limits.max * scale};
}

JPH::EMotorState motorState(JointMotorMode mode)
{
    switch (mode) {
    case JointMotorMode::Off: return JPH::EMotorState::Off;
    case JointMotorMode::Velocity: return JPH::EMotorState::Velocity;
    case JointMotorMode::Position: return JPH::EMotorState::Position;
    }

    return JPH::EMotorState::Off;
}

// Настройки ограничения Jolt по описанию скрепления и рамкам первого и
// второго тела в мире. Пределы, пружины и моторы ставит configure.
class SettingsBuilder {
public:
    SettingsBuilder(const JointFrame& first, const JointFrame& second)
        : m_first(first)
        , m_second(second)
    {
    }

    JPH::Ref<JPH::TwoBodyConstraintSettings> operator()(const FixedJointDefinition&) const
    {
        auto settings = new JPH::FixedConstraintSettings;
        settings->mPoint1 = JPH::RVec3(toJolt(m_first.point));
        settings->mAxisX1 = toJolt(m_first.normal);
        settings->mAxisY1 = toJolt(m_first.axis);
        settings->mPoint2 = JPH::RVec3(toJolt(m_second.point));
        settings->mAxisX2 = toJolt(m_second.normal);
        settings->mAxisY2 = toJolt(m_second.axis);

        return settings;
    }

    JPH::Ref<JPH::TwoBodyConstraintSettings> operator()(const PointJointDefinition&) const
    {
        auto settings = new JPH::PointConstraintSettings;
        settings->mPoint1 = JPH::RVec3(toJolt(m_first.point));
        settings->mPoint2 = JPH::RVec3(toJolt(m_second.point));

        return settings;
    }

    JPH::Ref<JPH::TwoBodyConstraintSettings> operator()(const HingeJointDefinition&) const
    {
        auto settings = new JPH::HingeConstraintSettings;
        settings->mPoint1 = JPH::RVec3(toJolt(m_first.point));
        settings->mHingeAxis1 = toJolt(m_first.axis);
        settings->mNormalAxis1 = toJolt(m_first.normal);
        settings->mPoint2 = JPH::RVec3(toJolt(m_second.point));
        settings->mHingeAxis2 = toJolt(m_second.axis);
        settings->mNormalAxis2 = toJolt(m_second.normal);

        return settings;
    }

    JPH::Ref<JPH::TwoBodyConstraintSettings> operator()(const SliderJointDefinition&) const
    {
        auto settings = new JPH::SliderConstraintSettings;
        settings->mPoint1 = JPH::RVec3(toJolt(m_first.point));
        settings->mSliderAxis1 = toJolt(m_first.axis);
        settings->mNormalAxis1 = toJolt(m_first.normal);
        settings->mPoint2 = JPH::RVec3(toJolt(m_second.point));
        settings->mSliderAxis2 = toJolt(m_second.axis);
        settings->mNormalAxis2 = toJolt(m_second.normal);

        return settings;
    }

    JPH::Ref<JPH::TwoBodyConstraintSettings> operator()(const DistanceJointDefinition& definition) const
    {
        auto settings = new JPH::DistanceConstraintSettings;
        settings->mPoint1 = JPH::RVec3(toJolt(m_first.point));
        settings->mPoint2 = JPH::RVec3(toJolt(m_second.point));
        if (definition.minDistance >= 0.0f || definition.maxDistance >= 0.0f) {
            settings->mMinDistance = std::max(definition.minDistance, 0.0f);
            settings->mMaxDistance = std::max(definition.maxDistance, settings->mMinDistance);
        }

        return settings;
    }

    JPH::Ref<JPH::TwoBodyConstraintSettings> operator()(const ConeJointDefinition&) const
    {
        auto settings = new JPH::ConeConstraintSettings;
        settings->mPoint1 = JPH::RVec3(toJolt(m_first.point));
        settings->mTwistAxis1 = toJolt(m_first.axis);
        settings->mPoint2 = JPH::RVec3(toJolt(m_second.point));
        settings->mTwistAxis2 = toJolt(m_second.axis);

        return settings;
    }

    JPH::Ref<JPH::TwoBodyConstraintSettings> operator()(const SwingTwistJointDefinition&) const
    {
        auto settings = new JPH::SwingTwistConstraintSettings;
        settings->mPosition1 = JPH::RVec3(toJolt(m_first.point));
        settings->mTwistAxis1 = toJolt(m_first.axis);
        settings->mPlaneAxis1 = toJolt(m_first.normal);
        settings->mPosition2 = JPH::RVec3(toJolt(m_second.point));
        settings->mTwistAxis2 = toJolt(m_second.axis);
        settings->mPlaneAxis2 = toJolt(m_second.normal);

        return settings;
    }

    JPH::Ref<JPH::TwoBodyConstraintSettings> operator()(const SixDofJointDefinition&) const
    {
        auto settings = new JPH::SixDOFConstraintSettings;
        settings->mPosition1 = JPH::RVec3(toJolt(m_first.point));
        settings->mAxisX1 = toJolt(m_first.normal);
        settings->mAxisY1 = toJolt(m_first.axis);
        settings->mPosition2 = JPH::RVec3(toJolt(m_second.point));
        settings->mAxisX2 = toJolt(m_second.normal);
        settings->mAxisY2 = toJolt(m_second.axis);
        // Пирамида: пределы качания могут быть несимметричными.
        settings->mSwingType = JPH::ESwingType::Pyramid;

        return settings;
    }

private:
    const JointFrame& m_first;
    const JointFrame& m_second;
};

// Пределы, пружины и моторы на готовое ограничение.
void configureConstraint(const JointDefinition& definition, JPH::TwoBodyConstraint& constraint)
{
    if (const auto* hinge = std::get_if<HingeJointDefinition>(&definition)) {
        auto& joint = static_cast<JPH::HingeConstraint&>(constraint);
        const auto [min, max] = hingeLimits(hinge->limits);
        joint.SetLimits(min, max);
        joint.SetLimitsSpringSettings(springOf(hinge->limitSpring));
        joint.GetMotorSettings().SetTorqueLimit(hinge->motor.maxForce);
        joint.SetMotorState(motorState(hinge->motor.mode));
        joint.SetTargetAngularVelocity(glm::radians(hinge->motor.target));
        joint.SetTargetAngle(glm::radians(hinge->motor.target));
    } else if (const auto* slider = std::get_if<SliderJointDefinition>(&definition)) {
        auto& joint = static_cast<JPH::SliderConstraint&>(constraint);
        const auto [min, max] = sliderLimits(slider->limits);
        joint.SetLimits(min, max);
        joint.SetLimitsSpringSettings(springOf(slider->limitSpring));
        joint.GetMotorSettings().SetForceLimit(slider->motor.maxForce);
        joint.SetMotorState(motorState(slider->motor.mode));
        joint.SetTargetVelocity(slider->motor.target);
        joint.SetTargetPosition(slider->motor.target);
    } else if (const auto* distance = std::get_if<DistanceJointDefinition>(&definition)) {
        auto& joint = static_cast<JPH::DistanceConstraint&>(constraint);
        if (distance->minDistance >= 0.0f || distance->maxDistance >= 0.0f) {
            const float min = std::max(distance->minDistance, 0.0f);
            joint.SetDistance(min, std::max(distance->maxDistance, min));
        }
        joint.SetLimitsSpringSettings(springOf(distance->spring));
    } else if (const auto* cone = std::get_if<ConeJointDefinition>(&definition)) {
        static_cast<JPH::ConeConstraint&>(constraint).SetHalfConeAngle(glm::radians(cone->halfConeAngle));
    } else if (const auto* swingTwist = std::get_if<SwingTwistJointDefinition>(&definition)) {
        auto& joint = static_cast<JPH::SwingTwistConstraint&>(constraint);
        joint.SetNormalHalfConeAngle(glm::radians(swingTwist->normalHalfConeAngle));
        joint.SetPlaneHalfConeAngle(glm::radians(swingTwist->planeHalfConeAngle));
        joint.SetTwistMinAngle(glm::radians(swingTwist->twistMin));
        joint.SetTwistMaxAngle(glm::radians(swingTwist->twistMax));
        joint.SetMaxFrictionTorque(swingTwist->maxFrictionTorque);
        joint.GetSwingMotorSettings().SetTorqueLimit(swingTwist->maxMotorTorque);
        joint.GetTwistMotorSettings().SetTorqueLimit(swingTwist->maxMotorTorque);
        const JPH::EMotorState state = motorState(swingTwist->motor);
        joint.SetSwingMotorState(state);
        joint.SetTwistMotorState(state);
        const Vec3 radians = glm::radians(swingTwist->motorTarget);
        joint.SetTargetAngularVelocityCS(toJolt(radians));
        // Углы мотора: кручение вокруг X рамки, качание вокруг Y и Z.
        joint.SetTargetOrientationCS(
            JPH::Quat::sRotation(JPH::Vec3::sAxisX(), radians.x) * JPH::Quat::sRotation(JPH::Vec3::sAxisY(), radians.y)
            * JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), radians.z)
        );
    } else if (const auto* sixDof = std::get_if<SixDofJointDefinition>(&definition)) {
        auto& joint = static_cast<JPH::SixDOFConstraint&>(constraint);
        const AxisLocks& linear = sixDof->lockLinear;
        const AxisLocks& angular = sixDof->lockAngular;
        constexpr float kMeters = 1.0f;
        const float radians = glm::radians(1.0f);
        const auto [linearMinX, linearMaxX] = axisLimits(linear.x, sixDof->linearX, kMeters);
        const auto [linearMinY, linearMaxY] = axisLimits(linear.y, sixDof->linearY, kMeters);
        const auto [linearMinZ, linearMaxZ] = axisLimits(linear.z, sixDof->linearZ, kMeters);
        joint.SetTranslationLimits(
            JPH::Vec3(linearMinX, linearMinY, linearMinZ),
            JPH::Vec3(linearMaxX, linearMaxY, linearMaxZ)
        );
        const auto [angularMinX, angularMaxX] = axisLimits(angular.x, sixDof->angularX, radians);
        const auto [angularMinY, angularMaxY] = axisLimits(angular.y, sixDof->angularY, radians);
        const auto [angularMinZ, angularMaxZ] = axisLimits(angular.z, sixDof->angularZ, radians);
        joint.SetRotationLimits(
            JPH::Vec3(angularMinX, angularMinY, angularMinZ),
            JPH::Vec3(angularMaxX, angularMaxY, angularMaxZ)
        );
        for (const auto axis :
             {JPH::SixDOFConstraint::EAxis::TranslationX,
              JPH::SixDOFConstraint::EAxis::TranslationY,
              JPH::SixDOFConstraint::EAxis::TranslationZ}) {
            joint.SetLimitsSpringSettings(axis, springOf(sixDof->linearSpring));
        }
    }
}

float length(const JPH::Vector<2>& value)
{
    return std::sqrt(value[0] * value[0] + value[1] * value[1]);
}

float combined(float a, float b, float c)
{
    return std::sqrt(a * a + b * b + c * c);
}

// Импульсы последнего шага: на линейные части (сила) и на поворот (момент).
std::pair<float, float> impulsesOf(const JPH::TwoBodyConstraint& constraint)
{
    switch (constraint.GetSubType()) {
    case JPH::EConstraintSubType::Fixed: {
        const auto& joint = static_cast<const JPH::FixedConstraint&>(constraint);

        return {joint.GetTotalLambdaPosition().Length(), joint.GetTotalLambdaRotation().Length()};
    }
    case JPH::EConstraintSubType::Point: {
        return {static_cast<const JPH::PointConstraint&>(constraint).GetTotalLambdaPosition().Length(), 0.0f};
    }
    case JPH::EConstraintSubType::Hinge: {
        const auto& joint = static_cast<const JPH::HingeConstraint&>(constraint);
        const float rotation = combined(
            length(joint.GetTotalLambdaRotation()),
            joint.GetTotalLambdaRotationLimits(),
            joint.GetTotalLambdaMotor()
        );

        return {joint.GetTotalLambdaPosition().Length(), rotation};
    }
    case JPH::EConstraintSubType::Slider: {
        const auto& joint = static_cast<const JPH::SliderConstraint&>(constraint);
        const float position = combined(
            length(joint.GetTotalLambdaPosition()),
            joint.GetTotalLambdaPositionLimits(),
            joint.GetTotalLambdaMotor()
        );

        return {position, joint.GetTotalLambdaRotation().Length()};
    }
    case JPH::EConstraintSubType::Distance: {
        return {std::abs(static_cast<const JPH::DistanceConstraint&>(constraint).GetTotalLambdaPosition()), 0.0f};
    }
    case JPH::EConstraintSubType::Cone: {
        const auto& joint = static_cast<const JPH::ConeConstraint&>(constraint);

        return {joint.GetTotalLambdaPosition().Length(), std::abs(joint.GetTotalLambdaRotation())};
    }
    case JPH::EConstraintSubType::SwingTwist: {
        const auto& joint = static_cast<const JPH::SwingTwistConstraint&>(constraint);
        const float limits = combined(
            joint.GetTotalLambdaTwist(),
            joint.GetTotalLambdaSwingY(),
            joint.GetTotalLambdaSwingZ()
        );

        return {joint.GetTotalLambdaPosition().Length(), std::hypot(limits, joint.GetTotalLambdaMotor().Length())};
    }
    case JPH::EConstraintSubType::SixDOF: {
        const auto& joint = static_cast<const JPH::SixDOFConstraint&>(constraint);
        const float linear = std::hypot(
            joint.GetTotalLambdaPosition().Length(),
            joint.GetTotalLambdaMotorTranslation().Length()
        );
        const float angular = std::hypot(
            joint.GetTotalLambdaRotation().Length(),
            joint.GetTotalLambdaMotorRotation().Length()
        );

        return {linear, angular};
    }
    default: break;
    }

    return {0.0f, 0.0f};
}

} // namespace

PhysicsJoints::PhysicsJoints(Scene& scene, JPH::PhysicsSystem& system, PhysicsBodies& bodies, CollisionIgnores& ignores)
    : m_scene(scene)
    , m_system(system)
    , m_bodies(bodies)
    , m_ignores(ignores)
{
    m_bodies.setBodyRemovedListener([this](JPH::BodyID id) {
        bodyRemoved(id);
    });
}

PhysicsJoints::~PhysicsJoints()
{
    m_bodies.setBodyRemovedListener({});
    for (auto& [id, entry] : m_joints) {
        remove(entry);
    }
}

void PhysicsJoints::changed(Joint& joint)
{
    Entry& entry = m_joints[joint.id()];
    entry.joint = &joint;
    entry.dirty = true;
    // Точки и оси снова берутся по сцене.
    entry.connectedFrame.reset();
}

void PhysicsJoints::tuned(Joint& joint)
{
    const auto found = m_joints.find(joint.id());
    if (found != m_joints.end()) {
        found->second.tune = true;
    }
}

void PhysicsJoints::left(const Joint& joint)
{
    const auto found = m_joints.find(joint.id());
    if (found == m_joints.end()) {
        return;
    }
    remove(found->second);
    m_joints.erase(found);
}

void PhysicsJoints::bodyRemoved(JPH::BodyID id)
{
    for (auto& [jointId, entry] : m_joints) {
        if (entry.constraint && (entry.own == id || entry.connected == id)) {
            remove(entry);
        }
    }
}

void PhysicsJoints::remove(Entry& entry)
{
    if (entry.ignoring) {
        m_ignores.removeBodyPair(entry.ownOwner, entry.connectedOwner);
        entry.ignoring = false;
    }
    if (entry.constraint) {
        m_system.RemoveConstraint(entry.constraint);
        entry.constraint = nullptr;
    }
    entry.force = 0.0f;
    entry.torque = 0.0f;
}

bool PhysicsJoints::firstWarning(ComponentId id)
{
    return m_warned.insert(id).second;
}

void PhysicsJoints::sync()
{
    if (!m_scene.playing()) {
        for (auto& [id, entry] : m_joints) {
            remove(entry);
        }

        return;
    }
    for (auto& [id, entry] : m_joints) {
        Joint& joint = *entry.joint;
        Node* node = joint.owner();
        const JPH::BodyID own = node && joint.activeAndEnabled() ? m_bodies.bodyOf(node->id()) : JPH::BodyID();
        if (own.IsInvalid()) {
            remove(entry);
            continue;
        }
        const NodeId connectedOwner = joint.connectedBody().id;
        JPH::BodyID connected;
        if (connectedOwner != 0) {
            connected = m_bodies.bodyOf(connectedOwner);
            if (connected.IsInvalid()) {
                if (firstWarning(id)) {
                    log::warn(
                        "Physics: the joint on node '{}' waits for its connected node to have a body",
                        node->name()
                    );
                }
                remove(entry);
                continue;
            }
        }
        if (entry.constraint && !entry.dirty && entry.own == own && entry.connected == connected) {
            if (entry.tune) {
                configure(entry);
            }
            continue;
        }
        remove(entry);
        build(entry, own, connected, connectedOwner);
    }
}

void PhysicsJoints::build(Entry& entry, JPH::BodyID own, JPH::BodyID connected, NodeId connectedOwner)
{
    const Joint& joint = *entry.joint;
    const auto* ownNode = dynamic_cast<const Node3D*>(joint.owner());
    const auto* connectedNode = connectedOwner != 0 ? dynamic_cast<const Node3D*>(m_scene.find(connectedOwner))
                                                    : nullptr;
    if (!ownNode || (connectedOwner != 0 && !connectedNode)) {
        return;
    }
    const JointDefinition definition = joint.definition();
    const Vec3 axis = jointAxis(definition);
    const JointFrame ownWorld = toWorld(
        ownNode->worldMatrix(),
        {.point = joint.anchor(), .axis = axis, .normal = jointNormal(axis)}
    );
    const Mat4 connectedMatrix = connectedNode ? connectedNode->worldMatrix() : Mat4(1.0f);
    if (!entry.connectedFrame || entry.connectedOwner != connectedOwner) {
        entry.connectedFrame = toLocal(connectedMatrix, ownWorld);
    }
    JointFrame connectedWorld = toWorld(connectedMatrix, *entry.connectedFrame);
    if (const auto* distance = std::get_if<DistanceJointDefinition>(&definition)) {
        connectedWorld.point = Vec3(connectedMatrix * Vec4(distance->connectedAnchor, 1.0f));
    }

    // Первое тело - другое (или мир), второе - своё: угол петли и сдвиг
    // слайдера меряются для своего тела вдоль его оси.
    const JPH::Ref<JPH::TwoBodyConstraintSettings> settings = std::visit(
        SettingsBuilder(connectedWorld, ownWorld),
        definition
    );
    settings->mNumVelocityStepsOverride = static_cast<JPH::uint>(joint.solverStepsOverride());
    settings->mNumPositionStepsOverride = static_cast<JPH::uint>(joint.solverStepsOverride());
    JPH::BodyInterface& bodies = m_system.GetBodyInterface();
    entry.constraint = bodies.CreateConstraint(settings, connected, own);
    if (!entry.constraint) {
        return;
    }
    m_system.AddConstraint(entry.constraint);
    entry.own = own;
    entry.connected = connected;
    entry.ownOwner = ownNode->id();
    entry.connectedOwner = connectedOwner;
    entry.dirty = false;
    if (!joint.collideConnected() && connectedOwner != 0) {
        m_ignores.addBodyPair(entry.ownOwner, connectedOwner);
        entry.ignoring = true;
    }
    configure(entry);
}

void PhysicsJoints::configure(Entry& entry)
{
    entry.tune = false;
    const Joint& joint = *entry.joint;
    JPH::TwoBodyConstraint& constraint = *entry.constraint;
    configureConstraint(joint.definition(), constraint);
    constraint.SetNumVelocityStepsOverride(static_cast<JPH::uint>(joint.solverStepsOverride()));
    constraint.SetNumPositionStepsOverride(static_cast<JPH::uint>(joint.solverStepsOverride()));
    m_system.GetBodyInterface().ActivateConstraint(&constraint);
}

void PhysicsJoints::afterStep(float stepSeconds)
{
    std::vector<Entry*> broken;
    for (auto& [id, entry] : m_joints) {
        if (!entry.constraint) {
            continue;
        }
        const auto [linear, angular] = impulsesOf(*entry.constraint);
        // Импульс шага, делённый на шаг, - средняя сила шага.
        entry.force = linear / stepSeconds;
        entry.torque = angular / stepSeconds;
        const Joint& joint = *entry.joint;
        const bool overForce = joint.breakForce() > 0.0f && entry.force > joint.breakForce();
        const bool overTorque = joint.breakTorque() > 0.0f && entry.torque > joint.breakTorque();
        if (overForce || overTorque) {
            broken.push_back(&entry);
        }
    }
    for (Entry* entry : broken) {
        remove(*entry);
        Joint& joint = *entry->joint;
        joint.setEnabled(false);
        joint.onBreak.emit();
    }
}

const PhysicsJoints::Entry* PhysicsJoints::find(const Joint& joint) const
{
    const auto found = m_joints.find(joint.id());

    return found != m_joints.end() && found->second.constraint ? &found->second : nullptr;
}

float PhysicsJoints::force(const Joint& joint) const
{
    const Entry* entry = find(joint);

    return entry ? entry->force : 0.0f;
}

float PhysicsJoints::torque(const Joint& joint) const
{
    const Entry* entry = find(joint);

    return entry ? entry->torque : 0.0f;
}

float PhysicsJoints::hingeAngle(const Joint& joint) const
{
    const Entry* entry = find(joint);
    if (!entry || entry->constraint->GetSubType() != JPH::EConstraintSubType::Hinge) {
        return 0.0f;
    }

    return static_cast<const JPH::HingeConstraint&>(*entry->constraint).GetCurrentAngle();
}

float PhysicsJoints::sliderPosition(const Joint& joint) const
{
    const Entry* entry = find(joint);
    if (!entry || entry->constraint->GetSubType() != JPH::EConstraintSubType::Slider) {
        return 0.0f;
    }

    return static_cast<const JPH::SliderConstraint&>(*entry->constraint).GetCurrentPosition();
}

} // namespace yqlora
