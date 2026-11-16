// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsDebug.hpp"
#include "yqlora/physics/PhysicsWorld.hpp"
#include "yqlora/physics/PhysicsWorldImpl.hpp"

#include "yqlora/debug/DebugLines.hpp"
#include "yqlora/physics/JoltMath.hpp"

#include <Jolt/Geometry/AABox.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace yqlora {

namespace {

const Vec4 kStaticColor(0.45f, 0.8f, 0.45f, 1.0f);
const Vec4 kKinematicColor(0.45f, 0.65f, 1.0f, 1.0f);
const Vec4 kDynamicColor(1.0f, 0.75f, 0.3f, 1.0f);
const Vec4 kSleepingColor(0.55f, 0.55f, 0.6f, 1.0f);
const Vec4 kTriggerColor(1.0f, 0.9f, 0.2f, 1.0f);
const Vec4 kContactColor(1.0f, 0.25f, 0.25f, 1.0f);
const Vec4 kVelocityColor(0.25f, 0.9f, 1.0f, 1.0f);

// Крест точки касания и стрелка нормали, м.
constexpr float kContactPointSize = 0.1f;
constexpr float kContactNormalLength = 0.4f;
// Стрелка скорости - путь тела за это время, с; медленнее kMinSpeed, м/с, - без стрелки.
constexpr float kVelocityArrowSeconds = 0.25f;
constexpr float kMinSpeed = 0.01f;
// Треугольников меша за один вызов GetTrianglesNext.
constexpr int kTriangleBatch = JPH::Shape::cGetTrianglesMinTrianglesRequested;

class LeafCollector final : public JPH::TransformedShapeCollector {
public:
    void AddHit(const JPH::TransformedShape& shape) override
    {
        leaves.push_back(shape);
    }

    std::vector<JPH::TransformedShape> leaves;
};

float largest(const Vec3& scale)
{
    return std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)});
}

// Простые фигуры - своими линиями, остальное (выпуклая оболочка, меш) -
// рёбрами треугольников фигуры.
void drawLeaf(const JPH::TransformedShape& leaf, DebugLines& lines, const DebugPen& pen)
{
    const Vec3 center = fromJolt(JPH::Vec3(leaf.mShapePositionCOM));
    const Quat rotation = fromJolt(leaf.mShapeRotation);
    const Vec3 scale = fromJolt(leaf.GetShapeScale());
    const JPH::Shape& shape = *leaf.mShape;
    switch (shape.GetSubType()) {
    case JPH::EShapeSubType::Sphere: {
        lines.sphere(center, static_cast<const JPH::SphereShape&>(shape).GetRadius() * largest(scale), pen);

        return;
    }
    case JPH::EShapeSubType::Box: {
        const Vec3 half = fromJolt(static_cast<const JPH::BoxShape&>(shape).GetHalfExtent()) * glm::abs(scale);
        lines.box(center, half, rotation, pen);

        return;
    }
    case JPH::EShapeSubType::Capsule: {
        // Jolt масштабирует капсулу только поровну по осям.
        const auto& capsule = static_cast<const JPH::CapsuleShape&>(shape);
        const float size = std::abs(scale.x);
        const float radius = capsule.GetRadius() * size;
        lines.capsule(center, radius, 2.0f * (capsule.GetHalfHeightOfCylinder() * size + radius), rotation, pen);

        return;
    }
    case JPH::EShapeSubType::Cylinder: {
        const auto& cylinder = static_cast<const JPH::CylinderShape&>(shape);
        const float height = 2.0f * cylinder.GetHalfHeight() * std::abs(scale.y);
        lines.cylinder(center, cylinder.GetRadius() * std::abs(scale.x), height, rotation, pen);

        return;
    }
    default: break;
    }

    JPH::Shape::GetTrianglesContext context;
    leaf.GetTrianglesStart(context, JPH::AABox::sBiggest(), JPH::RVec3::sZero());
    std::array<JPH::Float3, kTriangleBatch * 3> vertices;
    while (const int count = leaf.GetTrianglesNext(context, kTriangleBatch, vertices.data())) {
        for (int i = 0; i < count; ++i) {
            const auto at = [&](int corner) {
                const JPH::Float3& vertex = vertices[static_cast<std::size_t>(i * 3 + corner)];

                return Vec3(vertex.x, vertex.y, vertex.z);
            };
            lines.triangle(at(0), at(1), at(2), pen);
        }
    }
}

void drawShape(const JPH::Body& body, DebugLines& lines, const DebugPen& pen)
{
    LeafCollector collector;
    body.GetTransformedShape().CollectTransformedShapes(JPH::AABox::sBiggest(), collector);
    for (const JPH::TransformedShape& leaf : collector.leaves) {
        drawLeaf(leaf, lines, pen);
    }
}

Vec4 motionColor(const JPH::Body& body)
{
    switch (body.GetMotionType()) {
    case JPH::EMotionType::Static: return kStaticColor;
    case JPH::EMotionType::Kinematic: return kKinematicColor;
    case JPH::EMotionType::Dynamic: return kDynamicColor;
    }

    return kStaticColor;
}

struct BodyChannels {
    bool colliders = false;
    bool triggers = false;
    bool velocities = false;
    bool sleep = false;

    bool any() const
    {
        return colliders || triggers || velocities || sleep;
    }
};

void drawBody(const JPH::Body& body, const BodyChannels& channels, DebugLines& lines)
{
    if (body.IsSensor()) {
        if (channels.triggers) {
            drawShape(body, lines, {.color = kTriggerColor});
        }

        return;
    }
    const bool sleeping = !body.IsStatic() && !body.IsActive();
    if (sleeping && channels.sleep) {
        drawShape(body, lines, {.color = kSleepingColor});
    } else if (channels.colliders) {
        drawShape(body, lines, {.color = motionColor(body)});
    }
    if (channels.velocities && body.IsDynamic()) {
        const Vec3 velocity = fromJolt(body.GetLinearVelocity());
        if (glm::length(velocity) >= kMinSpeed) {
            const Vec3 from = fromJolt(JPH::Vec3(body.GetCenterOfMassPosition()));
            lines.arrow(from, from + velocity * kVelocityArrowSeconds, {.color = kVelocityColor});
        }
    }
}

void drawContacts(const PhysicsContacts& contacts, DebugLines& lines)
{
    const DebugPen pen{.color = kContactColor};
    for (const auto& [key, touch] : contacts.collisions()) {
        for (const Vec3& point : touch.points) {
            lines.point(point, kContactPointSize, pen);
            lines.arrow(point, point + touch.normal * kContactNormalLength, pen);
        }
    }
}

} // namespace

void PhysicsWorld::drawDebug(DebugLines& lines, const DebugChannels& channels)
{
    const BodyChannels bodies{
        .colliders = channels.contains(kCollidersDebugChannel),
        .triggers = channels.contains(kTriggersDebugChannel),
        .velocities = channels.contains(kVelocitiesDebugChannel),
        .sleep = channels.contains(kSleepDebugChannel),
    };
    const bool contacts = channels.contains(kContactsDebugChannel);
    if (!bodies.any() && !contacts) {
        return;
    }
    // Тела - на месте узлов, как для запроса.
    Impl* world = readyForQuery();
    if (!world) {
        return;
    }
    if (bodies.any()) {
        JPH::BodyIDVector ids;
        world->system.GetBodies(ids);
        const JPH::BodyLockInterface& locks = world->system.GetBodyLockInterface();
        for (const JPH::BodyID id : ids) {
            const JPH::BodyLockRead lock(locks, id);
            if (lock.Succeeded()) {
                drawBody(lock.GetBody(), bodies, lines);
            }
        }
    }
    if (contacts) {
        drawContacts(world->contacts, lines);
    }
}

void PhysicsWorld::addDebugChannels(DebugChannels& channels) const
{
    for (const std::string_view channel : kPhysicsDebugChannels) {
        channels.emplace(channel);
    }
}

} // namespace yqlora
