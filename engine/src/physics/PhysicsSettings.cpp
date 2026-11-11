#include "yqlora/physics/PhysicsSettings.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/project/Project.hpp"

#include <algorithm>

namespace yqlora {

void PhysicsSettings::setVelocitySteps(int steps)
{
    m_velocitySteps = std::clamp(steps, kMinSolverSteps, kMaxSolverSteps);
}

void PhysicsSettings::setPositionSteps(int steps)
{
    m_positionSteps = std::clamp(steps, kMinSolverSteps, kMaxSolverSteps);
}

const PhysicsSettings& physicsSettingsOf(const EngineServices& services)
{
    static const PhysicsSettings defaults;
    const Project* project = services.project();
    const SettingsSection* section = project ? project->settings().find(typeNameOf<PhysicsSettings>()) : nullptr;

    return section ? static_cast<const PhysicsSettings&>(*section) : defaults;
}

} // namespace yqlora
