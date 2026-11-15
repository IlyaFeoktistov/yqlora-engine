// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsQueries.hpp"

#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/JoltShapes.hpp"
#include "yqlora/physics/PhysicsBodies.hpp"
#include "yqlora/physics/PhysicsLayers.hpp"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <glm/geometric.hpp>
#include <tuple>

namespace yqlora {

namespace {

// Слои узлов из маски запроса.
class MaskFilter final : public JPH::ObjectLayerFilter {
public:
    explicit MaskFilter(LayerMask mask)
        : m_mask(mask)
    {
    }

    bool ShouldCollide(JPH::ObjectLayer layer) const override
    {
        return inMask(m_mask, nodeLayerOf(static_cast<PhysicsObjectLayer>(layer)));
    }

private:
    LayerMask m_mask;
};

// Датчики триггеров - только по просьбе.
class TriggerFilter final : public JPH::BodyFilter {
public:
    explicit TriggerFilter(bool includeTriggers)
        : m_includeTriggers(includeTriggers)
    {
    }

    bool ShouldCollideLocked(const JPH::Body& body) const override
    {
        return m_includeTriggers || !body.IsSensor();
    }

private:
    bool m_includeTriggers;
};

// Направление луча единичное, длина пути - отдельно.
Vec3 unit(Vec3 direction)
{
    const float length = glm::length(direction);

    return length > 0.0f ? direction / length : Vec3(0.0f);
}

} // namespace

PhysicsQueries::PhysicsQueries(const JPH::PhysicsSystem& system, const PhysicsBodies& bodies)
    : m_system(system)
    , m_bodies(bodies)
{
}

std::optional<RaycastHit> PhysicsQueries::hitOf(
    const JPH::BodyID& body,
    const JPH::SubShapeID& part,
    Vec3 point,
    const std::optional<Vec3>& normal,
    float distance
) const
{
    const JPH::BodyLockRead lock(m_system.GetBodyLockInterface(), body);
    if (!lock.Succeeded()) {
        return std::nullopt;
    }
    const JPH::Body& locked = lock.GetBody();
    const auto side = m_bodies.sideOf(body.GetIndexAndSequenceNumber(), bodyPartOf(*locked.GetShape(), part));
    if (!side) {
        return std::nullopt;
    }

    return RaycastHit{
        .node = NodeRef{side->owner},
        .collider = {side->part.node, side->part.collider},
        .point = point,
        .normal = normal ? *normal : fromJolt(locked.GetWorldSpaceSurfaceNormal(part, toJolt(point))),
        .distance = distance,
    };
}

std::optional<RaycastHit> PhysicsQueries::raycast(
    Vec3 origin,
    Vec3 direction,
    float maxDistance,
    const QueryFilter& filter
) const
{
    const std::vector<RaycastHit> all = raycastAll(origin, direction, maxDistance, filter);
    if (all.empty()) {
        return std::nullopt;
    }

    return all.front();
}

std::vector<RaycastHit> PhysicsQueries::raycastAll(
    Vec3 origin,
    Vec3 direction,
    float maxDistance,
    const QueryFilter& filter
) const
{
    const Vec3 along = unit(direction) * maxDistance;
    const JPH::RRayCast ray{toJolt(origin), toJolt(along)};
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    const MaskFilter layers(filter.mask);
    const TriggerFilter triggers(filter.includeTriggers);
    m_system.GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings(), collector, {}, layers, triggers);
    collector.Sort();
    std::vector<RaycastHit> hits;
    for (const JPH::RayCastResult& result : collector.mHits) {
        const Vec3 point = origin + along * result.mFraction;
        if (auto
                hit = hitOf(result.mBodyID, result.mSubShapeID2, point, std::nullopt, maxDistance * result.mFraction)) {
            hits.push_back(*hit);
        }
    }

    return hits;
}

std::optional<RaycastHit> PhysicsQueries::shapeCast(
    const JPH::Shape& shape,
    Vec3 origin,
    Quat rotation,
    Vec3 direction,
    float maxDistance,
    const QueryFilter& filter
) const
{
    const Vec3 along = unit(direction) * maxDistance;
    const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(
        &shape,
        JPH::Vec3::sOne(),
        JPH::RMat44::sRotationTranslation(toJolt(rotation), toJolt(origin)),
        toJolt(along)
    );
    JPH::ShapeCastSettings settings;
    // Ближайшая точка касания, а не самая глубокая в начале пути.
    settings.mReturnDeepestPoint = false;
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    const MaskFilter layers(filter.mask);
    const TriggerFilter triggers(filter.includeTriggers);
    m_system.GetNarrowPhaseQuery().CastShape(cast, settings, JPH::RVec3::sZero(), collector, {}, layers, triggers);
    if (!collector.HadHit()) {
        return std::nullopt;
    }
    const JPH::ShapeCastResult& result = collector.mHit;
    // Ось проникновения смотрит из протянутой фигуры в коллайдер: нормаль коллайдера - против неё.
    const Vec3 normal = unit(-fromJolt(result.mPenetrationAxis));

    return hitOf(
        result.mBodyID2,
        result.mSubShapeID2,
        fromJolt(JPH::Vec3(result.mContactPointOn2)),
        normal,
        maxDistance * result.mFraction
    );
}

std::vector<ComponentRef<Collider>> PhysicsQueries::overlap(
    const JPH::Shape& shape,
    Vec3 center,
    Quat rotation,
    const QueryFilter& filter
) const
{
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    const MaskFilter layers(filter.mask);
    const TriggerFilter triggers(filter.includeTriggers);
    m_system.GetNarrowPhaseQuery().CollideShape(
        &shape,
        JPH::Vec3::sOne(),
        JPH::RMat44::sRotationTranslation(toJolt(rotation), toJolt(center)),
        JPH::CollideShapeSettings(),
        JPH::RVec3::sZero(),
        collector,
        {},
        layers,
        triggers
    );
    std::vector<ComponentRef<Collider>> colliders;
    for (const JPH::CollideShapeResult& result : collector.mHits) {
        const auto hit = hitOf(result.mBodyID2, result.mSubShapeID2, center, Vec3(0.0f), 0.0f);
        if (hit && std::ranges::find(colliders, hit->collider) == colliders.end()) {
            colliders.push_back(hit->collider);
        }
    }
    std::ranges::sort(colliders, [](const ComponentRef<Collider>& a, const ComponentRef<Collider>& b) {
        return std::tie(a.node, a.component) < std::tie(b.node, b.component);
    });

    return colliders;
}

} // namespace yqlora
