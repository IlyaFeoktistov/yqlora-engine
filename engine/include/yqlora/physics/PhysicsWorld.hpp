#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/TypeName.hpp"
#include "yqlora/scene/SceneSystem.hpp"

#include <cstdint>
#include <memory>

namespace yqlora {

// Физический мир сцены (Jolt). Шагает в afterFixedUpdate, после
// onFixedUpdate компонентов, по шагу TimeSettings::fixedStep; шагает только
// идущая сцена (копия Play, игра), потому что advance зовут только у неё.
// Сам мир Jolt создаётся с первым телом: сцена без физики его не держит.
// Устройство - docs/architecture/physics.md.
class YQLORA_API PhysicsWorld final : public SceneSystem {
public:
    YQLORA_TYPE(PhysicsWorld, SceneSystem)

    explicit PhysicsWorld(Scene& scene);
    ~PhysicsWorld() override;

    void afterFixedUpdate(float stepSeconds) override;
    // Сглаженные тела показывают положение между двумя последними шагами.
    void update(float deltaSeconds) override;
    void onBranchChanged(Node& node) override;

    // Есть ли мир Jolt (появилось ли тело).
    bool simulating() const
    {
        return m_impl != nullptr;
    }

    // Сколько шагов мир сделал с создания.
    std::uint64_t stepCount() const
    {
        return m_steps;
    }

    // Гравитация мира, м/с^2: сначала из настроек "Физика"; setGravity
    // меняет её до конца сцены, изменение настроек ставит снова их значение.
    Vec3 gravity() const;
    void setGravity(const Vec3& gravity);

private:
    friend struct PhysicsWorldAccess;
    struct Impl;

    // Создаёт мир Jolt, если его ещё нет.
    Impl& impl();

    std::unique_ptr<Impl> m_impl;
    std::uint64_t m_steps = 0;
    Vec3 m_gravity;
    // Гравитация настроек, которую мир видел последней: её изменение
    // применяется поверх значения из кода.
    Vec3 m_settingsGravity;
};

} // namespace yqlora
