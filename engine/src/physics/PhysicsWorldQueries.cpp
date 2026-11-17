// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/JoltShapes.hpp"
#include "yqlora/physics/PhysicsQueryDebug.hpp"
#include "yqlora/physics/PhysicsWorld.hpp"
#include "yqlora/physics/PhysicsWorldImpl.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>

namespace yqlora {

namespace {

// Фигура запроса - той же постройки, что фигура коллайдера.
JPH::RefConst<JPH::Shape> queryShape(const ColliderShape& shape)
{
    const PlacedShape placed{.shape = shape};

    return joltBodyShape(std::span(&placed, 1), {}).shape;
}

// Капсула по всей высоте с полусферами, как у CapsuleCollider.
CapsuleShape capsule(float radius, float height)
{
    return {radius, std::max(height * 0.5f - radius, 0.0f)};
}

const Quat kNoRotation(1.0f, 0.0f, 0.0f, 0.0f);

QueryDebug::Shape debugSphere(float radius)
{
    return [radius](DebugDraw& draw, const Vec3& center, const DebugOptions& options) {
        draw.sphere(center, radius, options);
    };
}

QueryDebug::Shape debugBox(const Vec3& halfExtents, const Quat& rotation)
{
    return [halfExtents, rotation](DebugDraw& draw, const Vec3& center, const DebugOptions& options) {
        draw.box(center, halfExtents, rotation, options);
    };
}

QueryDebug::Shape debugCapsule(float radius, float height, const Quat& rotation)
{
    return [radius, height, rotation](DebugDraw& draw, const Vec3& center, const DebugOptions& options) {
        draw.capsule(center, radius, height, rotation, options);
    };
}

std::span<const RaycastHit> hitsOf(const std::optional<RaycastHit>& hit)
{
    return hit ? std::span(&*hit, 1) : std::span<const RaycastHit>{};
}

} // namespace

PhysicsWorld::Impl* PhysicsWorld::readyForQuery()
{
    if (!m_impl) {
        return nullptr;
    }
    m_impl->bodies.flush();
    if (!scene().playing()) {
        m_impl->bodies.placeMoved();
    }
    m_impl->chains.sync();
    m_impl->characters.sync();

    return m_impl.get();
}

std::optional<RaycastHit> PhysicsWorld::raycast(
    const Vec3& origin,
    const Vec3& direction,
    float maxDistance,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();
    const auto hit = world ? world->queries.raycast(origin, direction, maxDistance, filter) : std::nullopt;
    QueryDebug(scene()).ray(origin, direction, maxDistance, hitsOf(hit));

    return hit;
}

std::vector<RaycastHit> PhysicsWorld::raycastAll(
    const Vec3& origin,
    const Vec3& direction,
    float maxDistance,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();
    auto hits = world ? world->queries.raycastAll(origin, direction, maxDistance, filter) : std::vector<RaycastHit>{};
    QueryDebug(scene()).ray(origin, direction, maxDistance, hits);

    return hits;
}

std::optional<RaycastHit> PhysicsWorld::sphereCast(
    const Vec3& origin,
    float radius,
    const Vec3& direction,
    float maxDistance,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();
    std::optional<RaycastHit> hit;
    if (world) {
        const auto shape = queryShape(SphereShape{radius});
        hit = world->queries.shapeCast(*shape, origin, kNoRotation, direction, maxDistance, filter);
    }
    QueryDebug(scene()).cast(debugSphere(radius), origin, direction, maxDistance, hit);

    return hit;
}

std::optional<RaycastHit> PhysicsWorld::boxCast(
    const Vec3& center,
    const Vec3& halfExtents,
    const Quat& rotation,
    const Vec3& direction,
    float maxDistance,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();
    std::optional<RaycastHit> hit;
    if (world) {
        const auto shape = queryShape(BoxShape{halfExtents});
        hit = world->queries.shapeCast(*shape, center, rotation, direction, maxDistance, filter);
    }
    QueryDebug(scene()).cast(debugBox(halfExtents, rotation), center, direction, maxDistance, hit);

    return hit;
}

std::optional<RaycastHit> PhysicsWorld::capsuleCast(
    const Vec3& center,
    float radius,
    float height,
    const Quat& rotation,
    const Vec3& direction,
    float maxDistance,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();
    std::optional<RaycastHit> hit;
    if (world) {
        const auto shape = queryShape(capsule(radius, height));
        hit = world->queries.shapeCast(*shape, center, rotation, direction, maxDistance, filter);
    }
    QueryDebug(scene()).cast(debugCapsule(radius, height, rotation), center, direction, maxDistance, hit);

    return hit;
}

std::vector<ComponentRef<Collider>> PhysicsWorld::overlapSphere(
    const Vec3& center,
    float radius,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();
    std::vector<ComponentRef<Collider>> found;
    if (world) {
        const auto shape = queryShape(SphereShape{radius});
        found = world->queries.overlap(*shape, center, kNoRotation, filter);
    }
    QueryDebug(scene()).overlap(debugSphere(radius), center, !found.empty());

    return found;
}

std::vector<ComponentRef<Collider>> PhysicsWorld::overlapBox(
    const Vec3& center,
    const Vec3& halfExtents,
    const Quat& rotation,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();
    std::vector<ComponentRef<Collider>> found;
    if (world) {
        const auto shape = queryShape(BoxShape{halfExtents});
        found = world->queries.overlap(*shape, center, rotation, filter);
    }
    QueryDebug(scene()).overlap(debugBox(halfExtents, rotation), center, !found.empty());

    return found;
}

std::vector<ComponentRef<Collider>> PhysicsWorld::overlapCapsule(
    const Vec3& center,
    float radius,
    float height,
    const Quat& rotation,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();
    std::vector<ComponentRef<Collider>> found;
    if (world) {
        const auto shape = queryShape(capsule(radius, height));
        found = world->queries.overlap(*shape, center, rotation, filter);
    }
    QueryDebug(scene()).overlap(debugCapsule(radius, height, rotation), center, !found.empty());

    return found;
}

void PhysicsWorld::ignoreCollision(
    const ComponentRef<Collider>& first,
    const ComponentRef<Collider>& second,
    bool ignore
)
{
    Impl& world = impl();
    if (world.ignores.set(first.component, second.component, ignore)) {
        // Спящая пара иначе не проверялась бы заново.
        world.bodies.wake(first.component);
        world.bodies.wake(second.component);
    }
}

bool PhysicsWorld::collisionIgnored(const ComponentRef<Collider>& first, const ComponentRef<Collider>& second) const
{
    return m_impl && m_impl->ignores.ignored(first.component, second.component);
}

} // namespace yqlora
