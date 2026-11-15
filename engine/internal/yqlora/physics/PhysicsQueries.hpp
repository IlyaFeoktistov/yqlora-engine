#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/physics/PhysicsQuery.hpp"

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <optional>
#include <vector>

namespace JPH {

class PhysicsSystem;

} // namespace JPH

namespace yqlora {

class PhysicsBodies;

// Запросы к миру Jolt: луч, протягивание фигуры, пересечение фигуры с
// коллайдерами. Только читает мир; тела перед запросом готовит PhysicsWorld.
class PhysicsQueries {
public:
    PhysicsQueries(const JPH::PhysicsSystem& system, const PhysicsBodies& bodies);

    std::optional<RaycastHit> raycast(Vec3 origin, Vec3 direction, float maxDistance, const QueryFilter& filter) const;
    // Все попадания, ближние первыми.
    std::vector<RaycastHit> raycastAll(Vec3 origin, Vec3 direction, float maxDistance, const QueryFilter& filter) const;

    // Фигура shape с поворотом rotation протягивается из origin.
    std::optional<RaycastHit> shapeCast(
        const JPH::Shape& shape,
        Vec3 origin,
        Quat rotation,
        Vec3 direction,
        float maxDistance,
        const QueryFilter& filter
    ) const;

    // Коллайдеры, которые пересекает фигура, по ID узлов и коллайдеров.
    std::vector<ComponentRef<Collider>> overlap(
        const JPH::Shape& shape,
        Vec3 center,
        Quat rotation,
        const QueryFilter& filter
    ) const;

private:
    std::optional<RaycastHit> hitOf(
        const JPH::BodyID& body,
        const JPH::SubShapeID& part,
        Vec3 point,
        const std::optional<Vec3>& normal,
        float distance
    ) const;

    const JPH::PhysicsSystem& m_system;
    const PhysicsBodies& m_bodies;
};

} // namespace yqlora
