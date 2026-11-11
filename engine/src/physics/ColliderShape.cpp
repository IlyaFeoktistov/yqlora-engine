#include "yqlora/physics/ColliderShape.hpp"

#include <algorithm>
#include <cmath>

namespace yqlora {

namespace {

// Оси, которые отличаются меньше, - один масштаб: числа из матриц не бывают
// ровными.
constexpr float kScaleTolerance = 1e-4f;

bool same(float a, float b)
{
    return std::abs(a - b) <= kScaleTolerance * std::max(std::abs(a), std::abs(b));
}

struct Scaled {
    Vec3 scale;
    bool& uniformLost;

    ColliderShape operator()(const BoxShape& box) const
    {
        return BoxShape{box.halfExtents * scale};
    }

    ColliderShape operator()(const SphereShape& sphere) const
    {
        const float largest = std::max({scale.x, scale.y, scale.z});
        uniformLost = !same(scale.x, scale.y) || !same(scale.y, scale.z);

        return SphereShape{sphere.radius * largest};
    }

    ColliderShape operator()(const CapsuleShape& capsule) const
    {
        uniformLost = !same(scale.x, scale.z);
        // Растянутая по Y капсула остаётся капсулой с круглыми полусферами:
        // высота целиком умножается на Y, радиус - на наибольшую из X и Z.
        const float radius = capsule.radius * std::max(scale.x, scale.z);
        const float height = (capsule.halfHeight + capsule.radius) * scale.y;

        return CapsuleShape{radius, std::max(height - radius, 0.0f)};
    }

    ColliderShape operator()(const CylinderShape& cylinder) const
    {
        uniformLost = !same(scale.x, scale.z);

        return CylinderShape{cylinder.radius * std::max(scale.x, scale.z), cylinder.halfHeight * scale.y};
    }

    ColliderShape operator()(const MeshShape& mesh) const
    {
        return mesh;
    }
};

} // namespace

ColliderShape scaledShape(const ColliderShape& shape, const Vec3& scale, bool& uniformLost)
{
    uniformLost = false;

    return std::visit(Scaled{scale, uniformLost}, shape);
}

} // namespace yqlora
