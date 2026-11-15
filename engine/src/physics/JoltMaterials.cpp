// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/JoltMaterials.hpp"

#include <Jolt/Physics/Body/Body.h>

namespace yqlora {

namespace {

const PhysicsMaterialData& materialOf(const JPH::Body& body, const JPH::SubShapeID& part)
{
    return static_cast<const JoltPhysicsMaterial*>(body.GetShape()->GetMaterial(part))->data();
}

} // namespace

float combineJoltFriction(
    const JPH::Body& first,
    const JPH::SubShapeID& firstPart,
    const JPH::Body& second,
    const JPH::SubShapeID& secondPart
)
{
    return combinedFriction(materialOf(first, firstPart), materialOf(second, secondPart));
}

float combineJoltRestitution(
    const JPH::Body& first,
    const JPH::SubShapeID& firstPart,
    const JPH::Body& second,
    const JPH::SubShapeID& secondPart
)
{
    return combinedRestitution(materialOf(first, firstPart), materialOf(second, secondPart));
}

} // namespace yqlora
