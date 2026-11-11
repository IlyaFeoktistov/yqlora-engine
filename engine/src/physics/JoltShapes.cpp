// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/JoltShapes.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/physics/JoltMath.hpp"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <algorithm>
#include <vector>

namespace yqlora {

namespace {

// Короче прямая часть капсулы - это уже шар.
constexpr float kMinCapsuleHalfHeight = 0.0005f;

// Скругление рёбер ящика и цилиндра, как по умолчанию у Jolt, но не больше
// половины самого малого размера: иначе фигура вырождена.
float convexRadius(float smallestHalfSize)
{
    return std::min(JPH::cDefaultConvexRadius, smallestHalfSize * 0.5f);
}

JPH::RefConst<JPH::Shape> created(const JPH::ShapeSettings& settings)
{
    const JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) {
        log::error("Physics: a collider shape is not made: {}", result.GetError().c_str());

        return nullptr;
    }

    return result.Get();
}

struct ShapeOf {
    JPH::RefConst<JPH::Shape> operator()(const BoxShape& box) const
    {
        const float smallest = std::min({box.halfExtents.x, box.halfExtents.y, box.halfExtents.z});

        return created(JPH::BoxShapeSettings(toJolt(box.halfExtents), convexRadius(smallest)));
    }

    JPH::RefConst<JPH::Shape> operator()(const SphereShape& sphere) const
    {
        return created(JPH::SphereShapeSettings(sphere.radius));
    }

    JPH::RefConst<JPH::Shape> operator()(const CapsuleShape& capsule) const
    {
        if (capsule.halfHeight < kMinCapsuleHalfHeight) {
            return created(JPH::SphereShapeSettings(capsule.radius));
        }

        return created(JPH::CapsuleShapeSettings(capsule.halfHeight, capsule.radius));
    }

    JPH::RefConst<JPH::Shape> operator()(const CylinderShape& cylinder) const
    {
        const float smallest = std::min(cylinder.halfHeight, cylinder.radius);

        return created(JPH::CylinderShapeSettings(cylinder.halfHeight, cylinder.radius, convexRadius(smallest)));
    }
};

bool atOrigin(const PlacedShape& placed)
{
    return placed.position == Vec3(0.0f) && placed.rotation == Quat(1.0f, 0.0f, 0.0f, 0.0f);
}

} // namespace

JPH::RefConst<JPH::Shape> joltBodyShape(std::span<const PlacedShape> shapes)
{
    std::vector<std::pair<const PlacedShape*, JPH::RefConst<JPH::Shape>>> made;
    for (const PlacedShape& placed : shapes) {
        if (JPH::RefConst<JPH::Shape> shape = std::visit(ShapeOf{}, placed.shape)) {
            made.emplace_back(&placed, std::move(shape));
        }
    }
    if (made.empty()) {
        return new JPH::EmptyShape();
    }
    if (made.size() == 1) {
        const auto& [placed, shape] = made.front();
        if (atOrigin(*placed)) {
            return shape;
        }

        return created(JPH::RotatedTranslatedShapeSettings(toJolt(placed->position), toJolt(placed->rotation), shape));
    }
    JPH::StaticCompoundShapeSettings compound;
    for (const auto& [placed, shape] : made) {
        compound.AddShape(toJolt(placed->position), toJolt(placed->rotation), shape);
    }

    return created(compound);
}

} // namespace yqlora
