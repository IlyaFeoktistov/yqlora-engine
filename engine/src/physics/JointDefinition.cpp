#include "yqlora/physics/JointDefinition.hpp"

#include <glm/geometric.hpp>

#include <cmath>

namespace yqlora {

namespace {

const Vec3 kDefaultAxis(0.0f, 1.0f, 0.0f);

} // namespace

Vec3 jointAxis(const JointDefinition& definition)
{
    return std::visit(
        [](const auto& kind) {
            if constexpr (requires { kind.axis; }) {
                return glm::normalize(kind.axis);
            } else {
                return kDefaultAxis;
            }
        },
        definition
    );
}

Vec3 jointNormal(const Vec3& axis)
{
    const Vec3 helper = std::abs(axis.x) < 1.0f - math::kEpsilon ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 0.0f, 1.0f);

    return glm::normalize(glm::cross(glm::cross(axis, helper), axis));
}

} // namespace yqlora
