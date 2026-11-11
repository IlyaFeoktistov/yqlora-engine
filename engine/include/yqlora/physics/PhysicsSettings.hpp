#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/physics/LayerCollisionMatrix.hpp"
#include "yqlora/project/SettingsSection.hpp"

namespace yqlora {

class EngineServices;

// Земное ускорение свободного падения, м/с^2, вниз по Y.
inline constexpr float kEarthGravity = 9.81f;
inline constexpr Vec3 kDefaultGravity{0.0f, -kEarthGravity, 0.0f};
// Итерации решателя по скорости и по положению, как по умолчанию у Jolt:
// больше - устойчивее длинные цепи и стопки, но дороже.
inline constexpr int kDefaultVelocitySteps = 10;
inline constexpr int kDefaultPositionSteps = 2;
inline constexpr int kMinSolverSteps = 1;
inline constexpr int kMaxSolverSteps = 255;

// Раздел Physics настроек проекта: гравитация, итерации решателя и какие
// слои сталкиваются. Применяется сразу, в том числе во время Play: мир
// читает его перед каждым шагом.
class YQLORA_API PhysicsSettings final : public SettingsSection {
public:
    YQLORA_TYPE(PhysicsSettings, SettingsSection)

    const Vec3& gravity() const
    {
        return m_gravity;
    }

    void setGravity(const Vec3& gravity)
    {
        m_gravity = gravity;
    }

    int velocitySteps() const
    {
        return m_velocitySteps;
    }

    int positionSteps() const
    {
        return m_positionSteps;
    }

    // Прижимаются к [kMinSolverSteps, kMaxSolverSteps].
    void setVelocitySteps(int steps);
    void setPositionSteps(int steps);

    LayerCollisionMatrix& collisions()
    {
        return m_collisions;
    }

    const LayerCollisionMatrix& collisions() const
    {
        return m_collisions;
    }

private:
    Vec3 m_gravity = kDefaultGravity;
    int m_velocitySteps = kDefaultVelocitySteps;
    int m_positionSteps = kDefaultPositionSteps;
    LayerCollisionMatrix m_collisions;
};

// Раздел Physics открытого проекта, а без проекта - значения по умолчанию.
YQLORA_API const PhysicsSettings& physicsSettingsOf(const EngineServices& services);

} // namespace yqlora
