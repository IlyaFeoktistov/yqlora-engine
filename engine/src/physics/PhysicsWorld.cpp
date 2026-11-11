// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsWorld.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/physics/JoltLayers.hpp"
#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/PhysicsRuntime.hpp"
#include "yqlora/physics/PhysicsSettings.hpp"
#include "yqlora/physics/PhysicsWorldAccess.hpp"
#include "yqlora/scene/Scene.hpp"

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

struct PhysicsWorld::Impl {
    explicit Impl(PhysicsRuntime& physicsRuntime, const PhysicsSettings& settings)
        : runtime(physicsRuntime)
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
    }

    PhysicsRuntime& runtime;
    // Фильтры живут дольше системы: она хранит ссылки на них.
    JoltBroadPhaseLayers broadPhaseLayers;
    JoltObjectVsBroadPhase objectVsBroadPhase;
    JoltObjectPairs pairs;
    JPH::PhysicsSystem system;
    // Ошибки шага, о которых лог уже сказал: каждая - один раз.
    JPH::EPhysicsUpdateError reported = JPH::EPhysicsUpdateError::None;
};

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
        m_impl = std::make_unique<Impl>(services.physics(), physicsSettingsOf(services));
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

    const JPH::EPhysicsUpdateError errors = world.system.Update(
        stepSeconds,
        kCollisionStepsPerStep,
        &world.runtime.tempAllocator(),
        &world.runtime.jobs()
    );
    ++m_steps;
    // У флагов Jolt нет operator~: новые ошибки - побитово по числам.
    using Flags = std::underlying_type_t<JPH::EPhysicsUpdateError>;
    const auto fresh = static_cast<Flags>(static_cast<Flags>(errors) & ~static_cast<Flags>(world.reported));
    if (fresh != 0) {
        world.reported = world.reported | static_cast<JPH::EPhysicsUpdateError>(fresh);
        log::warn("Physics step overflowed (Jolt error flags {}): raise the world limits or reduce contacts", fresh);
    }
}

void PhysicsWorldAccess::ensureWorld(PhysicsWorld& world)
{
    world.impl();
}

} // namespace yqlora
