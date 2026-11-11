// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/JoltLayers.hpp"

namespace yqlora {

namespace {

JPH::BroadPhaseLayer toJolt(PhysicsBroadPhase broadPhase)
{
    return JPH::BroadPhaseLayer(static_cast<JPH::BroadPhaseLayer::Type>(broadPhase));
}

} // namespace

JPH::uint JoltBroadPhaseLayers::GetNumBroadPhaseLayers() const
{
    return kPhysicsBroadPhaseCount;
}

JPH::BroadPhaseLayer JoltBroadPhaseLayers::GetBroadPhaseLayer(JPH::ObjectLayer layer) const
{
    return toJolt(broadPhaseOf(layer));
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* JoltBroadPhaseLayers::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const
{
    return layer == toJolt(PhysicsBroadPhase::Moving) ? "Moving" : "Static";
}
#endif

bool JoltObjectVsBroadPhase::ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const
{
    return physicsLayerMeetsBroadPhase(layer, static_cast<PhysicsBroadPhase>(broadPhase.GetValue()));
}

bool JoltObjectPairs::ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const
{
    return physicsLayersCollide(matrix, a, b);
}

} // namespace yqlora
