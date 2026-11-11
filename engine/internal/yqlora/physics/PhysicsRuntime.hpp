#pragma once

#include "yqlora/core/Export.hpp"

#include <memory>

namespace JPH {

class JobSystem;
class TempAllocator;

} // namespace JPH

namespace yqlora {

class MeshShapeCache;

// Среда Jolt, общая для всех физических миров одних служб движка (и копии
// Play, которая делит их): система заданий, временный распределитель и
// формы из мешей.
// Создаётся при первом обращении, поэтому службы без физики ничего не
// платят; заодно держит глобальную настройку Jolt (JoltSetup).
class YQLORA_API PhysicsRuntime {
public:
    PhysicsRuntime();
    ~PhysicsRuntime();

    PhysicsRuntime(const PhysicsRuntime&) = delete;
    PhysicsRuntime& operator=(const PhysicsRuntime&) = delete;

    // Запускает среду, если ещё нет: до любого объекта Jolt, потому что он
    // берёт память через настроенный распределитель. Главный поток.
    void start();

    JPH::JobSystem& jobs();
    JPH::TempAllocator& tempAllocator();
    // Формы из мешей, общие для миров этих служб (сцены редактора и Play).
    MeshShapeCache& meshShapes();

    // Готова ли среда (было ли первое обращение).
    bool started() const
    {
        return m_state != nullptr;
    }

private:
    struct State;

    State& state();

    std::unique_ptr<State> m_state;
};

} // namespace yqlora
