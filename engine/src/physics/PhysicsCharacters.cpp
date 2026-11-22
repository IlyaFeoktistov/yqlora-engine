// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsCharacters.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/physics/CharacterController.hpp"
#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/JoltShapes.hpp"
#include "yqlora/physics/PhysicsBodies.hpp"
#include "yqlora/physics/PhysicsLayers.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/trigonometric.hpp>

#include <algorithm>

namespace yqlora {

namespace {

// Новая капсула resize может войти в стены не глубже этой доли зазора,
// как советует Jolt.
constexpr float kResizePenetrationShare = 1.5f;

Node3D* nodeOf(const CharacterController& controller)
{
    return dynamic_cast<Node3D*>(controller.owner());
}

// Половина прямой части капсулы, без полусфер.
float halfLineOf(float radius, float height)
{
    return std::max(height * 0.5f - radius, 0.0f);
}

// Капсула вертикально, сдвинутая на center от положения персонажа (узла).
// Капсула без прямой части - шар.
JPH::RefConst<JPH::Shape> capsuleShape(float radius, float height, const Vec3& center)
{
    const float halfLine = halfLineOf(radius, height);
    JPH::RefConst<JPH::Shape> core = halfLine > 0.0f
                                         ? JPH::RefConst<JPH::Shape>(new JPH::CapsuleShape(halfLine, radius))
                                         : JPH::RefConst<JPH::Shape>(new JPH::SphereShape(radius));

    return new JPH::RotatedTranslatedShape(toJolt(center), JPH::Quat::sIdentity(), core);
}

// Персонаж сталкивается как подвижное тело слоя своего узла.
PhysicsObjectLayer layerOf(const Node3D& node)
{
    return physicsObjectLayer(node.layer(), true);
}

// Скорость по земле на склоне с нормалью normal: KeepHorizontal ведёт её
// вдоль склона так, что по плану она та же; AlongSurface - той же длины
// вдоль склона.
Vec3 alongGround(const Vec3& horizontal, const Vec3& normal, SlopeSpeed mode)
{
    const float upward = glm::dot(normal, math::kWorldUp);
    if (upward < math::kEpsilon) {
        return horizontal;
    }
    Vec3 onSlope = horizontal - math::kWorldUp * (glm::dot(normal, horizontal) / upward);
    const float length = glm::length(onSlope);
    if (mode == SlopeSpeed::AlongSurface && length > math::kEpsilon) {
        onSlope *= glm::length(horizontal) / length;
    }

    return onSlope;
}

} // namespace

PhysicsCharacters::PhysicsCharacters(
    JPH::PhysicsSystem& system,
    PhysicsBodies& bodies,
    Filters filters,
    JPH::TempAllocator& allocator
)
    : m_system(system)
    , m_bodies(bodies)
    , m_filters(filters)
    , m_allocator(allocator)
{
}

PhysicsCharacters::~PhysicsCharacters()
{
    for (auto& [id, character] : m_characters) {
        destroy(character);
    }
}

void PhysicsCharacters::changed(CharacterController& controller)
{
    Character& character = m_characters[controller.id()];
    character.controller = &controller;
    character.dirty = true;
}

void PhysicsCharacters::left(const CharacterController& controller)
{
    const auto found = m_characters.find(controller.id());
    if (found == m_characters.end()) {
        return;
    }
    destroy(found->second);
    m_characters.erase(found);
}

void PhysicsCharacters::build(Character& character)
{
    const CharacterController& controller = *character.controller;
    const Node3D& node = *nodeOf(controller);
    const JPH::RefConst<JPH::Shape> shape = capsuleShape(controller.radius(), controller.height(), controller.center());
    // Опора - касания ниже центра нижней полусферы.
    const float support = controller.center().y - halfLineOf(controller.radius(), controller.height());

    JPH::CharacterVirtualSettings settings;
    settings.mShape = shape;
    settings.mInnerBodyShape = shape;
    settings.mInnerBodyLayer = layerOf(node);
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -support);
    settings.mMaxSlopeAngle = glm::radians(controller.slopeLimit());
    settings.mMass = controller.mass();
    settings.mMaxStrength = controller.pushForce();
    settings.mCharacterPadding = controller.skinWidth();
    character.virtualCharacter = new JPH::CharacterVirtual(
        &settings,
        toJolt(node.worldPosition()),
        JPH::Quat::sIdentity(),
        node.id(),
        &m_system
    );
    character.virtualCharacter->SetListener(this);
    character.layer = node.layer();
    character.placed = node.worldPosition();
    character.velocity = Vec3(0.0f);
    character.dirty = false;

    const JPH::BodyID inner = character.virtualCharacter->GetInnerBodyID();
    if (inner.IsInvalid()) {
        return;
    }
    // Датчики триггеров тоже кинематические: без этого флага персонаж в
    // зону не входил бы.
    {
        const JPH::BodyLockWrite lock(m_system.GetBodyLockInterface(), inner);
        if (lock.Succeeded()) {
            lock.GetBody().SetCollideKinematicVsNonDynamic(true);
        }
    }
    m_bodies.addCharacterBody(inner, node.id());
}

void PhysicsCharacters::destroy(Character& character)
{
    if (!character.virtualCharacter) {
        return;
    }
    const JPH::BodyID inner = character.virtualCharacter->GetInnerBodyID();
    if (!inner.IsInvalid()) {
        m_bodies.removeCharacterBody(inner);
    }
    // Персонаж Jolt убирает своё внутреннее тело сам.
    character.virtualCharacter = nullptr;
}

void PhysicsCharacters::sync()
{
    for (auto& [id, character] : m_characters) {
        const CharacterController& controller = *character.controller;
        const Node3D* node = nodeOf(controller);
        if (!node || !controller.activeAndEnabled()) {
            destroy(character);
            continue;
        }
        if (node->getComponent<RigidBody>() && m_warnings.first(id)) {
            log::warn(
                "Physics: node '{}' has both a CharacterController and a RigidBody; they would both move it",
                node->name()
            );
        }
        if (!character.virtualCharacter || character.dirty || character.layer != node->layer()) {
            destroy(character);
            build(character);
            continue;
        }
        const Vec3 position = node->worldPosition();
        if (position != character.placed) {
            character.virtualCharacter->SetPosition(toJolt(position));
            character.placed = position;
        }
    }
}

PhysicsCharacters::Character* PhysicsCharacters::ready(const CharacterController& controller)
{
    sync();
    const auto found = m_characters.find(controller.id());

    return found != m_characters.end() && found->second.virtualCharacter ? &found->second : nullptr;
}

void PhysicsCharacters::move(CharacterController& controller, const Vec3& velocity, float deltaSeconds)
{
    Character* character = ready(controller);
    if (!character || deltaSeconds <= 0.0f) {
        return;
    }
    JPH::CharacterVirtual& virtualCharacter = *character->virtualCharacter;
    virtualCharacter.SetMaxSlopeAngle(glm::radians(controller.slopeLimit()));
    virtualCharacter.SetMass(controller.mass());
    virtualCharacter.SetMaxStrength(controller.pushForce());
    // Платформа могла сдвинуться после прошлого движения.
    virtualCharacter.UpdateGroundVelocity();

    Vec3 wanted = velocity;
    if (virtualCharacter.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround) {
        const float vertical = glm::dot(velocity, math::kWorldUp);
        const Vec3 horizontal = velocity - math::kWorldUp * vertical;
        // Вниз на земле не идут: на спуске ноги держит stickToGround.
        wanted = alongGround(horizontal, fromJolt(virtualCharacter.GetGroundNormal()), controller.slopeSpeed())
                 + math::kWorldUp * std::max(vertical, 0.0f) + fromJolt(virtualCharacter.GetGroundVelocity());
    }
    virtualCharacter.SetLinearVelocity(toJolt(wanted));

    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    settings.mStickToFloorStepDown = controller.stickToGround() ? JPH::Vec3(0.0f, -controller.stickDistance(), 0.0f)
                                                                : JPH::Vec3::sZero();
    settings.mWalkStairsStepUp = controller.walkStairs() ? JPH::Vec3(0.0f, controller.stepHeight(), 0.0f)
                                                         : JPH::Vec3::sZero();
    Node3D& node = *nodeOf(controller);
    const PhysicsObjectLayer layer = layerOf(node);
    const JPH::DefaultBroadPhaseLayerFilter broadPhase(m_filters.broadPhase, layer);
    const JPH::DefaultObjectLayerFilter objects(m_filters.pairs, layer);
    const Vec3 from = fromJolt(virtualCharacter.GetPosition());
    m_hits.clear();
    virtualCharacter
        .ExtendedUpdate(deltaSeconds, m_system.GetGravity(), settings, broadPhase, objects, {}, {}, m_allocator);
    const Vec3 to = fromJolt(virtualCharacter.GetPosition());
    character->velocity = (to - from) / deltaSeconds;
    node.setWorldPosition(to);
    character->placed = node.worldPosition();
    emitHits(controller);
}

bool PhysicsCharacters::tryCapsule(const CharacterController& controller, float height, const Vec3& center)
{
    Character* character = ready(controller);
    if (!character) {
        return true;
    }
    JPH::CharacterVirtual& virtualCharacter = *character->virtualCharacter;
    const JPH::RefConst<JPH::Shape> shape = capsuleShape(controller.radius(), height, center);
    const PhysicsObjectLayer layer = layerOf(*nodeOf(controller));
    const JPH::DefaultBroadPhaseLayerFilter broadPhase(m_filters.broadPhase, layer);
    const JPH::DefaultObjectLayerFilter objects(m_filters.pairs, layer);
    const bool fits = virtualCharacter.SetShape(
        shape,
        kResizePenetrationShare * virtualCharacter.GetCharacterPadding(),
        broadPhase,
        objects,
        {},
        {},
        m_allocator
    );
    if (fits) {
        virtualCharacter.SetInnerBodyShape(shape);
    }

    return fits;
}

CharacterState PhysicsCharacters::state(const CharacterController& controller) const
{
    const auto found = m_characters.find(controller.id());
    if (found == m_characters.end() || !found->second.virtualCharacter) {
        return {};
    }
    const JPH::CharacterVirtual& virtualCharacter = *found->second.virtualCharacter;
    CharacterState state{.velocity = found->second.velocity};
    state.grounded = virtualCharacter.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
    if (virtualCharacter.IsSupported()) {
        state.groundNormal = fromJolt(virtualCharacter.GetGroundNormal());
        state.groundVelocity = fromJolt(virtualCharacter.GetGroundVelocity());
    }

    return state;
}

void PhysicsCharacters::OnContactAdded(
    const JPH::CharacterVirtual* /*character*/,
    const JPH::CharacterContact& contact,
    JPH::CharacterContactSettings& /*settings*/
)
{
    if (!contact.mIsSensorB && !contact.mBodyB.IsInvalid()) {
        m_hits.emplace_back(contact.mBodyB, contact.mSubShapeIDB);
    }
}

void PhysicsCharacters::emitHits(CharacterController& controller)
{
    const NodeId self = controller.owner()->id();
    std::vector<NodeId> touched;
    for (const auto& [id, part] : m_hits) {
        const JPH::BodyLockRead lock(m_system.GetBodyLockInterface(), id);
        if (!lock.Succeeded()) {
            continue;
        }
        const auto side = m_bodies.sideOf(id.GetIndexAndSequenceNumber(), bodyPartOf(*lock.GetBody().GetShape(), part));
        if (side && side->owner != self && std::ranges::find(touched, side->owner) == touched.end()) {
            touched.push_back(side->owner);
        }
    }
    m_hits.clear();
    for (const NodeId other : touched) {
        controller.onControllerHit.emit(NodeRef{other});
    }
}

} // namespace yqlora
