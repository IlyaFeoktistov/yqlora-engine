// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsRuntime.hpp"

#include "yqlora/physics/JoltSetup.hpp"

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>

#include <algorithm>
#include <thread>

namespace yqlora {

namespace {

// Память на один шаг всех миров служб, как в примерах Jolt: её хватает на
// десятки тысяч тел; больше - Jolt берёт память обычным путём и медленнее.
constexpr std::size_t kTempAllocatorBytes = std::size_t{10} * 1024 * 1024;

int workerThreads()
{
    // Главный поток тоже работает над шагом, поэтому рабочих - на одно ядро меньше.
    const int cores = static_cast<int>(std::thread::hardware_concurrency());

    return std::max(1, cores - 1);
}

} // namespace

struct PhysicsRuntime::State {
    // Настройка Jolt - первым членом: создаётся раньше и уходит позже всего,
    // что сделано через Jolt.
    JoltSetup setup;
    JPH::TempAllocatorImpl tempAllocator{kTempAllocatorBytes};
    JPH::JobSystemThreadPool jobs{JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, workerThreads()};
};

PhysicsRuntime::PhysicsRuntime() = default;

PhysicsRuntime::~PhysicsRuntime() = default;

void PhysicsRuntime::start()
{
    if (!m_state) {
        m_state = std::make_unique<State>();
    }
}

PhysicsRuntime::State& PhysicsRuntime::state()
{
    start();

    return *m_state;
}

JPH::JobSystem& PhysicsRuntime::jobs()
{
    return state().jobs;
}

JPH::TempAllocator& PhysicsRuntime::tempAllocator()
{
    return state().tempAllocator;
}

} // namespace yqlora
