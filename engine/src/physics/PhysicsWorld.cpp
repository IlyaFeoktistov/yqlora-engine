// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsWorld.hpp"
#include "yqlora/physics/PhysicsWorldImpl.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/physics/CharacterController.hpp"
#include "yqlora/physics/Joint.hpp"
#include "yqlora/physics/JoltContacts.hpp"
#include "yqlora/physics/JoltLayers.hpp"
#include "yqlora/physics/JoltMaterials.hpp"
#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/MeshShapeCache.hpp"
#include "yqlora/physics/PhysicsBodies.hpp"
#include "yqlora/physics/PhysicsContacts.hpp"
#include "yqlora/physics/PhysicsMaterialCache.hpp"
#include "yqlora/physics/PhysicsRuntime.hpp"
#include "yqlora/physics/PhysicsSettings.hpp"
#include "yqlora/physics/PhysicsWorldAccess.hpp"
#include "yqlora/scene/Scene.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <type_traits>

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
    world.characters.sync();
    PhysicsMaterialCache& materials = world.runtime.materials();
    materials.refresh(scene().services().resources());
    if (materials.generation() != world.materials) {
        world.materials = materials.generation();
        world.bodies.wakeAll();
    }
    world.bodies.beforeStep(stepSeconds);
    const JPH::EPhysicsUpdateError errors = world.system.Update(
        stepSeconds,
        kCollisionStepsPerStep,
        &world.runtime.tempAllocator(),
        &world.runtime.jobs()
    );
    ++m_steps;
    world.bodies.afterStep();
    world.joints.afterStep(stepSeconds);
    world.contacts.deliver(world.listener.take());
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
        m_impl->bodies.interpolate(scene().time().fixedStepFraction());
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

PhysicsWorld* worldOf(const Component& component)
{
    Node* node = component.owner();
    Scene* scene = node ? node->scene() : nullptr;

    return scene ? scene->system<PhysicsWorld>() : nullptr;
}

} // namespace

PhysicsCharacters* PhysicsWorldJolt::charactersOf(const Component& component)
{
    Node* node = component.owner();
    Scene* scene = node ? node->scene() : nullptr;
    PhysicsWorld* world = scene ? scene->system<PhysicsWorld>() : nullptr;

    return world ? &world->impl().characters : nullptr;
}

const PhysicsJoints* PhysicsWorldJolt::jointsOf(const Component& component)
{
    const Node* node = component.owner();
    const Scene* scene = node ? node->scene() : nullptr;
    const PhysicsWorld* world = scene ? scene->system<PhysicsWorld>() : nullptr;

    return world && world->m_impl ? &world->m_impl->joints : nullptr;
}

JoltWorld PhysicsWorldJolt::worldOf(const Component& component)
{
    Node* node = component.owner();
    Scene* scene = node ? node->scene() : nullptr;
    PhysicsWorld* world = scene ? scene->system<PhysicsWorld>() : nullptr;
    if (!world) {
        return {};
    }
    PhysicsWorld::Impl& impl = world->impl();
    impl.bodies.flush();

    return {&impl.system, &impl.bodies};
}

void PhysicsWorldAccess::changed(Component& component)
{
    PhysicsWorld* world = worldOf(component);
    if (!world) {
        return;
    }
    if (auto* controller = dynamic_cast<CharacterController*>(&component)) {
        world->impl().characters.changed(*controller);
    } else if (auto* joint = dynamic_cast<Joint*>(&component)) {
        world->impl().joints.changed(*joint);
    } else {
        world->impl().bodies.branchChanged(*component.owner());
    }
}

void PhysicsWorldAccess::left(Component& component)
{
    PhysicsWorld* world = worldOf(component);
    if (!world || !world->m_impl) {
        return;
    }
    if (const auto* controller = dynamic_cast<const CharacterController*>(&component)) {
        world->m_impl->characters.left(*controller);
    } else if (const auto* joint = dynamic_cast<const Joint*>(&component)) {
        world->m_impl->joints.left(*joint);
    } else {
        world->m_impl->bodies.componentLeft(component);
    }
}

void PhysicsWorldAccess::tuned(Component& component)
{
    PhysicsWorld* world = worldOf(component);
    auto* joint = dynamic_cast<Joint*>(&component);
    if (world && world->m_impl && joint) {
        world->m_impl->joints.tuned(*joint);
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
    const JPH::BodyID id = world.m_impl->bodies.bodyOf(owner);
    if (id.IsInvalid()) {
        return 0.0f;
    }
    const JPH::BodyLockRead lock(world.m_impl->system.GetBodyLockInterface(), id);
    const JPH::MotionProperties* motion = lock.Succeeded() ? lock.GetBody().GetMotionPropertiesUnchecked() : nullptr;
    if (!motion || motion->GetInverseMass() == 0.0f) {
        return 0.0f;
    }

    return 1.0f / motion->GetInverseMass();
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
