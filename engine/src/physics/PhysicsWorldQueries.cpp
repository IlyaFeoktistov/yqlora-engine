// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/JoltShapes.hpp"
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

    return world ? world->queries.raycast(origin, direction, maxDistance, filter) : std::nullopt;
}

std::vector<RaycastHit> PhysicsWorld::raycastAll(
    const Vec3& origin,
    const Vec3& direction,
    float maxDistance,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();

    return world ? world->queries.raycastAll(origin, direction, maxDistance, filter) : std::vector<RaycastHit>{};
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
    if (!world) {
        return std::nullopt;
    }
    const auto shape = queryShape(SphereShape{radius});

    return world->queries.shapeCast(*shape, origin, Quat(1.0f, 0.0f, 0.0f, 0.0f), direction, maxDistance, filter);
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
    if (!world) {
        return std::nullopt;
    }
    const auto shape = queryShape(BoxShape{halfExtents});

    return world->queries.shapeCast(*shape, center, rotation, direction, maxDistance, filter);
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
    if (!world) {
        return std::nullopt;
    }
    const auto shape = queryShape(capsule(radius, height));

    return world->queries.shapeCast(*shape, center, rotation, direction, maxDistance, filter);
}

std::vector<ComponentRef<Collider>> PhysicsWorld::overlapSphere(
    const Vec3& center,
    float radius,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();
    if (!world) {
        return {};
    }
    const auto shape = queryShape(SphereShape{radius});

    return world->queries.overlap(*shape, center, Quat(1.0f, 0.0f, 0.0f, 0.0f), filter);
}

std::vector<ComponentRef<Collider>> PhysicsWorld::overlapBox(
    const Vec3& center,
    const Vec3& halfExtents,
    const Quat& rotation,
    const QueryFilter& filter
)
{
    Impl* world = readyForQuery();
    if (!world) {
        return {};
    }
    const auto shape = queryShape(BoxShape{halfExtents});

    return world->queries.overlap(*shape, center, rotation, filter);
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
    if (!world) {
        return {};
    }
    const auto shape = queryShape(capsule(radius, height));

    return world->queries.overlap(*shape, center, rotation, filter);
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
