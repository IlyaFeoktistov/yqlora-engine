// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsWorld.hpp"
#include "yqlora/physics/PhysicsWorldImpl.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/physics/CharacterController.hpp"
#include "yqlora/physics/Collider.hpp"
#include "yqlora/physics/Joint.hpp"
#include "yqlora/physics/JoltBodyMass.hpp"
#include "yqlora/physics/JoltContacts.hpp"
#include "yqlora/physics/JoltLayers.hpp"
#include "yqlora/physics/JoltMaterials.hpp"
#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/MeshShapeCache.hpp"
#include "yqlora/physics/PhysicsBodies.hpp"
#include "yqlora/physics/PhysicsChain.hpp"
#include "yqlora/physics/PhysicsContacts.hpp"
#include "yqlora/physics/PhysicsMaterialCache.hpp"
#include "yqlora/physics/PhysicsRuntime.hpp"
#include "yqlora/physics/PhysicsSettings.hpp"
#include "yqlora/physics/PhysicsWorldAccess.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/scene/Scene.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <type_traits>
#include <vector>

namespace yqlora {

namespace {

// Пределы мира, как в примерах Jolt для больших сцен. Тела сверх
// kMaxBodies не создаются; пары и контакты сверх пределов теряются, а Jolt
// сообщает об этом ошибкой шага.
constexpr JPH::uint kMaxBodies = 65536;
// 0 - число мьютексов тел по умолчанию Jolt.
constexpr JPH::uint kBodyMutexes = 0;
constexpr JPH::uint kMaxBodyPairs = 65536;
constexpr JPH::uint kMaxContactConstraints = 10240;
// Шаг столкновений на фиксированный шаг: шаг уже мал (TimeSettings::fixedStep).
constexpr int kCollisionStepsPerStep = 1;

} // namespace

PhysicsWorld::Impl::Impl(Scene& scene, PhysicsRuntime& physicsRuntime, const PhysicsSettings& settings)
    : runtime(physicsRuntime)
    , listener(bodies, ignores)
    , bodies(scene, system.GetBodyInterface(), physicsRuntime.meshShapes(), physicsRuntime.materials())
    , characters(system, bodies, {objectVsBroadPhase, pairs}, physicsRuntime.tempAllocator())
    , joints(scene, system, bodies, ignores)
    , chains(scene, system, bodies, physicsRuntime.materials())
    , contacts(scene, bodies)
    , queries(system, bodies)
{
    // Среда запущена в PhysicsWorld::impl(), до создания членов Jolt.
    pairs.matrix = settings.collisions();
    system.Init(
        kMaxBodies,
        kBodyMutexes,
        kMaxBodyPairs,
        kMaxContactConstraints,
        broadPhaseLayers,
        objectVsBroadPhase,
        pairs
    );
    // Трение и упругость пары - из материалов подфигур контакта.
    system.SetCombineFriction(combineJoltFriction);
    system.SetCombineRestitution(combineJoltRestitution);
    system.SetContactListener(&listener);
}

PhysicsWorld::PhysicsWorld(Scene& scene)
    : SceneSystem(scene)
    , m_gravity(physicsSettingsOf(scene.services()).gravity())
    , m_settingsGravity(m_gravity)
{
}

PhysicsWorld::~PhysicsWorld() = default;

PhysicsWorld::Impl& PhysicsWorld::impl()
{
    if (!m_impl) {
        const EngineServices& services = scene().services();
        services.physics().start();
        m_impl = std::make_unique<Impl>(scene(), services.physics(), physicsSettingsOf(services));
        m_impl->system.SetGravity(toJolt(m_gravity));
    }

    return *m_impl;
}

Vec3 PhysicsWorld::gravity() const
{
    return m_impl ? fromJolt(m_impl->system.GetGravity()) : m_gravity;
}

void PhysicsWorld::setGravity(const Vec3& gravity)
{
    m_gravity = gravity;
    if (m_impl) {
        m_impl->system.SetGravity(toJolt(gravity));
    }
}

void PhysicsWorld::afterFixedUpdate(float stepSeconds)
{
    const PhysicsSettings& settings = physicsSettingsOf(scene().services());
    // Правка настроек во время Play применяется сразу, и поверх значения из кода.
    if (settings.gravity() != m_settingsGravity) {
        m_settingsGravity = settings.gravity();
        setGravity(m_settingsGravity);
    }
    if (!m_impl) {
        return;
    }
    Impl& world = *m_impl;
    world.pairs.matrix = settings.collisions();
    JPH::PhysicsSettings solver = world.system.GetPhysicsSettings();
    solver.mNumVelocitySteps = static_cast<JPH::uint>(settings.velocitySteps());
    solver.mNumPositionSteps = static_cast<JPH::uint>(settings.positionSteps());
    world.system.SetPhysicsSettings(solver);

    world.bodies.flush();
    world.joints.sync();
    world.chains.sync();
    world.characters.sync();
    PhysicsMaterialCache& materials = world.runtime.materials();
    materials.refresh(scene().services().resources());
    if (materials.generation() != world.materials) {
        world.materials = materials.generation();
        world.bodies.wakeAll();
        world.chains.wakeAll();
    }
    world.bodies.beforeStep(stepSeconds);
    world.chains.beforeStep();
    const JPH::EPhysicsUpdateError errors = world.system.Update(
        stepSeconds,
        kCollisionStepsPerStep,
        &world.runtime.tempAllocator(),
        &world.runtime.jobs()
    );
    ++m_steps;
    world.bodies.afterStep();
    world.joints.afterStep(stepSeconds);
    const std::vector<RawContact> touches = world.listener.take();
    world.chains.afterStep(touches);
    world.contacts.deliver(touches);
    // У флагов Jolt нет operator~: новые ошибки - побитово по числам.
    using Flags = std::underlying_type_t<JPH::EPhysicsUpdateError>;
    const auto fresh = static_cast<Flags>(static_cast<Flags>(errors) & ~static_cast<Flags>(world.reported));
    if (fresh != 0) {
        world.reported = world.reported | static_cast<JPH::EPhysicsUpdateError>(fresh);
        log::warn("Physics step overflowed (Jolt error flags {}): raise the world limits or reduce contacts", fresh);
    }
}

void PhysicsWorld::update(float /*deltaSeconds*/)
{
    if (m_impl) {
        const float fraction = scene().time().fixedStepFraction();
        m_impl->bodies.interpolate(fraction);
        m_impl->chains.interpolate(fraction);
    }
}

void PhysicsWorld::onBranchChanged(Node& node)
{
    // Без мира нет и тел: компонент физики, входя в сцену, сам его создаст.
    if (m_impl) {
        m_impl->bodies.branchChanged(node);
    }
}

void PhysicsWorldAccess::ensureWorld(PhysicsWorld& world)
{
    world.impl();
}

namespace {

PhysicsWorld* sceneWorldOf(const Component& component)
{
    Node* node = component.owner();
    Scene* scene = node ? node->scene() : nullptr;

    return scene ? scene->system<PhysicsWorld>() : nullptr;
}

} // namespace

PhysicsWorld::Impl* PhysicsWorldJolt::createdImplOf(const Component& component)
{
    PhysicsWorld* world = sceneWorldOf(component);

    return world ? &world->impl() : nullptr;
}

PhysicsWorld::Impl* PhysicsWorldJolt::existingImplOf(const Component& component)
{
    PhysicsWorld* world = sceneWorldOf(component);

    return world ? world->m_impl.get() : nullptr;
}

PhysicsCharacters* PhysicsWorldJolt::charactersOf(const Component& component)
{
    PhysicsWorld::Impl* impl = createdImplOf(component);

    return impl ? &impl->characters : nullptr;
}

const PhysicsJoints* PhysicsWorldJolt::jointsOf(const Component& component)
{
    const PhysicsWorld::Impl* impl = existingImplOf(component);

    return impl ? &impl->joints : nullptr;
}

const PhysicsChains* PhysicsWorldJolt::chainsOf(const Component& component)
{
    PhysicsWorld::Impl* impl = existingImplOf(component);
    if (!impl) {
        return nullptr;
    }
    // Звенья, построенные или перенесённые по изменениям после шага.
    impl->chains.sync();

    return &impl->chains;
}

JoltWorld PhysicsWorldJolt::worldOf(const Component& component)
{
    PhysicsWorld::Impl* impl = createdImplOf(component);
    if (!impl) {
        return {};
    }
    impl->bodies.flush();

    return {&impl->system, &impl->bodies};
}

void PhysicsWorldAccess::changed(Collider& collider)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::createdImplOf(collider)) {
        impl->bodies.branchChanged(*collider.owner());
    }
}

void PhysicsWorldAccess::changed(RigidBody& body)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::createdImplOf(body)) {
        impl->bodies.branchChanged(*body.owner());
    }
}

void PhysicsWorldAccess::changed(CharacterController& controller)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::createdImplOf(controller)) {
        impl->characters.changed(controller);
    }
}

void PhysicsWorldAccess::changed(Joint& joint)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::createdImplOf(joint)) {
        impl->joints.changed(joint);
    }
}

void PhysicsWorldAccess::changed(PhysicsChain& chain)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::createdImplOf(chain)) {
        impl->chains.changed(chain);
    }
}

void PhysicsWorldAccess::left(Collider& collider)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::existingImplOf(collider)) {
        impl->bodies.colliderLeft(collider);
    }
}

void PhysicsWorldAccess::left(RigidBody& body)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::existingImplOf(body)) {
        impl->bodies.rigidBodyLeft(body);
    }
}

void PhysicsWorldAccess::left(CharacterController& controller)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::existingImplOf(controller)) {
        impl->characters.left(controller);
    }
}

void PhysicsWorldAccess::left(Joint& joint)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::existingImplOf(joint)) {
        impl->joints.left(joint);
    }
}

void PhysicsWorldAccess::left(PhysicsChain& chain)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::existingImplOf(chain)) {
        impl->chains.left(chain);
    }
}

void PhysicsWorldAccess::tuned(Joint& joint)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::existingImplOf(joint)) {
        impl->joints.tuned(joint);
    }
}

void PhysicsWorldAccess::tuned(PhysicsChain& chain)
{
    if (PhysicsWorld::Impl* impl = PhysicsWorldJolt::existingImplOf(chain)) {
        impl->chains.tuned(chain);
    }
}

std::size_t PhysicsWorldAccess::bodyCount(const PhysicsWorld& world)
{
    return world.m_impl ? world.m_impl->bodies.bodyCount() : 0;
}

float PhysicsWorldAccess::bodyMass(const PhysicsWorld& world, NodeId owner)
{
    if (!world.m_impl) {
        return 0.0f;
    }

    return joltBodyMass(world.m_impl->system.GetBodyLockInterface(), world.m_impl->bodies.bodyOf(owner));
}

std::uint64_t PhysicsWorldAccess::meshShapesCooked(const PhysicsWorld& world)
{
    return world.m_impl ? world.m_impl->runtime.meshShapes().cooked() : 0;
}

void PhysicsWorldAccess::flush(PhysicsWorld& world)
{
    if (world.m_impl) {
        world.m_impl->bodies.flush();
    }
}

} // namespace yqlora
