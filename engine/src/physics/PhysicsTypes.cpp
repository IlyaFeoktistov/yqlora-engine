#include "yqlora/physics/PhysicsTypes.hpp"

#include "yqlora/physics/PhysicsSettings.hpp"
#include "yqlora/physics/PhysicsWorld.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"

#include <vector>

namespace yqlora {

void registerPhysicsTypes(TypeRegistry& registry)
{
    registry.registerSystem<PhysicsWorld>();

    registry.registerSettings<PhysicsSettings>("physics")
        .property("gravity", &PhysicsSettings::gravity, &PhysicsSettings::setGravity)
        .property("velocitySteps", &PhysicsSettings::velocitySteps, &PhysicsSettings::setVelocitySteps)
        .range(kMinSolverSteps, kMaxSolverSteps)
        .property("positionSteps", &PhysicsSettings::positionSteps, &PhysicsSettings::setPositionSteps)
        .range(kMinSolverSteps, kMaxSolverSteps)
        // В файле - пары слоёв, которые не сталкиваются; в окне - таблица.
        .property(
            "ignoredCollisions",
            [](const PhysicsSettings& settings) {
                return settings.collisions().ignoredPairs();
            },
            [](PhysicsSettings& settings, const std::vector<LayerPair>& pairs) {
                settings.collisions().setIgnoredPairs(pairs);
            },
            PropertyHint::CollisionMatrix
        );
}

} // namespace yqlora
