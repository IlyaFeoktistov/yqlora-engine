#include "yqlora/physics/PhysicsMaterialData.hpp"

#include <algorithm>

namespace yqlora {

PhysicsMaterialData sanitized(const PhysicsMaterialData& material)
{
    PhysicsMaterialData result = material;
    result.friction = std::max(result.friction, 0.0f);
    result.restitution = std::clamp(result.restitution, 0.0f, kMaxRestitution);

    return result;
}

PhysicsCombine combineMode(PhysicsCombine first, PhysicsCombine second)
{
    return std::max(first, second);
}

float combineValues(float first, float second, PhysicsCombine mode)
{
    switch (mode) {
    case PhysicsCombine::Average: return (first + second) * 0.5f;
    case PhysicsCombine::Minimum: return std::min(first, second);
    case PhysicsCombine::Multiply: return first * second;
    case PhysicsCombine::Maximum: return std::max(first, second);
    }

    return first;
}

float combinedFriction(const PhysicsMaterialData& first, const PhysicsMaterialData& second)
{
    return combineValues(first.friction, second.friction, combineMode(first.frictionCombine, second.frictionCombine));
}

float combinedRestitution(const PhysicsMaterialData& first, const PhysicsMaterialData& second)
{
    return combineValues(
        first.restitution,
        second.restitution,
        combineMode(first.restitutionCombine, second.restitutionCombine)
    );
}

} // namespace yqlora
