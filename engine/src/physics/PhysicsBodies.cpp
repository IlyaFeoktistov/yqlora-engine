// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsBodies.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/physics/Collider.hpp"
#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/JoltShapes.hpp"
#include "yqlora/physics/PhysicsLayers.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/Transform3D.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <algorithm>
#include <functional>

namespace yqlora {

namespace {

// Инерция тела без коллайдеров: как у шара такого радиуса, м.
constexpr float kBareBodyRadius = 0.5f;
// Момент инерции сплошного шара: 2/5 m r^2.
constexpr float kSolidSphereInertia = 0.4f;

const RigidBody* activeBodyOn(const Node& node)
{
    const RigidBody* body = node.getComponent<RigidBody>();

    return body && body->activeAndEnabled() ? body : nullptr;
}

JPH::EMotionType joltMotion(BodyMotion motion)
{
    switch (motion) {
    case BodyMotion::Static: return JPH::EMotionType::Static;
    case BodyMotion::Kinematic: return JPH::EMotionType::Kinematic;
    case BodyMotion::Dynamic: return JPH::EMotionType::Dynamic;
    }

    return JPH::EMotionType::Static;
}

int depthOf(const Node& node)
{
    int depth = 0;
    for (const Node* parent = node.parent(); parent; parent = parent->parent()) {
        ++depth;
    }

    return depth;
}

// Коллайдеры тела хозяина owner в порядке дерева: своего узла и, если у
// хозяина RigidBody, потомков до следующего узла со своим RigidBody.
void forEachBodyCollider(
    const Node& owner,
    bool withDescendants,
    const std::function<void(const Collider&, const Node&)>& visit
)
{
    const std::function<void(const Node&)> walk = [&](const Node& node) {
        for (const auto& component : node.components()) {
            const auto* collider = dynamic_cast<const Collider*>(component.get());
            if (collider && collider->activeAndEnabled()) {
                visit(*collider, node);
            }
        }
        if (!withDescendants) {
            return;
        }
        for (const auto& child : node.children()) {
            if (!activeBodyOn(*child)) {
                walk(*child);
            }
        }
    };
    walk(owner);
}

} // namespace

PhysicsBodies::PhysicsBodies(Scene& scene, JPH::BodyInterface& bodies)
    : m_scene(scene)
    , m_interface(bodies)
{
}

PhysicsBodies::~PhysicsBodies()
{
    for (const auto& [owner, body] : m_bodies) {
        m_interface.RemoveBody(body.id);
        m_interface.DestroyBody(body.id);
    }
}

void PhysicsBodies::branchChanged(const Node& node)
{
    m_dirtyBranches.push_back(node.id());
}

void PhysicsBodies::componentLeft(const Component& component)
{
    if (const auto it = m_colliderOwners.find(component.id()); it != m_colliderOwners.end()) {
        m_dirtyOwners.insert(it->second);
        m_colliderOwners.erase(it);
    }
    if (dynamic_cast<const RigidBody*>(&component) && component.owner()) {
        // Коллайдеры под ним достанутся телу выше или станут неподвижными.
        m_dirtyOwners.insert(component.owner()->id());
        m_dirtyBranches.push_back(component.owner()->id());
    }
}

NodeId PhysicsBodies::ownerOf(const Node& node) const
{
    for (const Node* current = &node; current; current = current->parent()) {
        if (activeBodyOn(*current)) {
            return current->id();
        }
    }

    return node.id();
}

std::vector<std::uint64_t> PhysicsBodies::childRevisionsOf(const Node& owner) const
{
    std::vector<std::uint64_t> revisions;
    forEachBodyCollider(owner, true, [&](const Collider& /*collider*/, const Node& node) {
        for (const Node* current = &node; current && current != &owner; current = current->parent()) {
            if (const auto* node3D = dynamic_cast<const Node3D*>(current)) {
                revisions.push_back(node3D->transform().revision());
            }
        }
    });

    return revisions;
}

void PhysicsBodies::flush()
{
    for (const NodeId branch : std::exchange(m_dirtyBranches, {})) {
        const Node* root = m_scene.find(branch);
        if (!root) {
            continue;
        }
        const std::function<void(const Node&)> walk = [&](const Node& node) {
            if (node.getComponent<RigidBody>()) {
                m_dirtyOwners.insert(node.id());
            }
            for (const auto& component : node.components()) {
                const auto* collider = dynamic_cast<const Collider*>(component.get());
                if (!collider) {
                    continue;
                }
                if (const auto it = m_colliderOwners.find(collider->id()); it != m_colliderOwners.end()) {
                    m_dirtyOwners.insert(it->second);
                    m_colliderOwners.erase(it);
                }
                if (collider->activeAndEnabled()) {
                    const NodeId owner = ownerOf(node);
                    m_colliderOwners[collider->id()] = owner;
                    m_dirtyOwners.insert(owner);
                }
            }
            for (const auto& child : node.children()) {
                walk(*child);
            }
        };
        walk(*root);
    }
    // Дочерний коллайдер сдвинули относительно узла тела: форма другая.
    for (auto& [owner, body] : m_bodies) {
        if (body.childRevisions.empty() || m_dirtyOwners.contains(owner)) {
            continue;
        }
        if (const Node* node = m_scene.find(owner); node && childRevisionsOf(*node) != body.childRevisions) {
            m_dirtyOwners.insert(owner);
        }
    }
    if (m_dirtyOwners.empty()) {
        return;
    }
    for (const NodeId owner : std::exchange(m_dirtyOwners, {})) {
        rebuild(owner);
    }
    sortByDepth();
}

void PhysicsBodies::remove(NodeId owner)
{
    const auto it = m_bodies.find(owner);
    if (it == m_bodies.end()) {
        return;
    }
    m_interface.RemoveBody(it->second.id);
    m_interface.DestroyBody(it->second.id);
    m_bodies.erase(it);
}

void PhysicsBodies::rebuild(NodeId owner)
{
    Node* node = m_scene.find(owner);
    auto* node3D = dynamic_cast<Node3D*>(node);
    const RigidBody* rigidBody = node ? activeBodyOn(*node) : nullptr;
    // Коллайдеры узла без своего RigidBody принадлежат телу выше, если оно есть.
    const bool ownsColliders = node3D && (rigidBody || ownerOf(*node) == owner);

    const Mat4 ownerWorld = node3D ? node3D->worldMatrix() : Mat4(1.0f);
    const Mat4 toOwner = glm::inverse(ownerWorld);
    std::vector<PlacedShape> shapes;
    if (ownsColliders) {
        forEachBodyCollider(*node, rigidBody != nullptr, [&](const Collider& collider, const Node& colliderNode) {
            const auto& colliderNode3D = static_cast<const Node3D&>(colliderNode);
            const Mat4 local = toOwner * colliderNode3D.worldMatrix() * glm::translate(Mat4(1.0f), collider.center())
                               * glm::mat4_cast(collider.rotation());
            const BodyPose pose = bodyPoseOf(local);
            shapes.push_back({collider.shape(), pose.position, pose.rotation});
            m_colliderOwners[collider.id()] = owner;
        });
    }
    if (!ownsColliders || (!rigidBody && shapes.empty())) {
        remove(owner);

        return;
    }

    // Тело делается заново, а скорость переходит к новому.
    JPH::Vec3 linearVelocity = JPH::Vec3::sZero();
    JPH::Vec3 angularVelocity = JPH::Vec3::sZero();
    if (const auto it = m_bodies.find(owner); it != m_bodies.end()) {
        linearVelocity = m_interface.GetLinearVelocity(it->second.id);
        angularVelocity = m_interface.GetAngularVelocity(it->second.id);
        remove(owner);
    }

    const BodyPose pose = bodyPoseOf(ownerWorld);
    const JPH::EMotionType motion = rigidBody ? joltMotion(rigidBody->motion()) : JPH::EMotionType::Static;
    const PhysicsObjectLayer layer = physicsObjectLayer(node->layer(), motion != JPH::EMotionType::Static);
    JPH::BodyCreationSettings
        settings(joltBodyShape(shapes), toJolt(pose.position), toJolt(pose.rotation), motion, layer);
    settings.mUserData = owner;
    if (rigidBody) {
        settings.mLinearDamping = rigidBody->linearDamping();
        settings.mAngularDamping = rigidBody->angularDamping();
        settings.mGravityFactor = rigidBody->gravityScale();
        const float mass = rigidBody->mass();
        if (shapes.empty()) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
            settings.mMassPropertiesOverride.mMass = mass;
            settings.mMassPropertiesOverride.mInertia = JPH::Mat44::sScale(
                kSolidSphereInertia * mass * kBareBodyRadius * kBareBodyRadius
            );
        } else {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = mass;
        }
    }
    const JPH::EActivation activation = motion == JPH::EMotionType::Static ? JPH::EActivation::DontActivate
                                                                           : JPH::EActivation::Activate;
    const JPH::BodyID id = m_interface.CreateAndAddBody(settings, activation);
    if (id.IsInvalid()) {
        log::error("Physics: no room for the body of node '{}', the world is full", node->name());

        return;
    }
    if (motion == JPH::EMotionType::Dynamic) {
        m_interface.SetLinearAndAngularVelocity(id, linearVelocity, angularVelocity);
    }
    m_bodies[owner] = Body{
        .id = id,
        .motion = motion,
        .interpolation = rigidBody && rigidBody->interpolation(),
        .depth = depthOf(*node),
        .remembered = pose,
        .previous = pose,
        .current = pose,
        .lastTarget = pose,
        .childRevisions = rigidBody ? childRevisionsOf(*node) : std::vector<std::uint64_t>{},
    };
}

void PhysicsBodies::sortByDepth()
{
    m_order.clear();
    for (const auto& [owner, body] : m_bodies) {
        m_order.push_back(owner);
    }
    std::ranges::sort(m_order, [&](NodeId a, NodeId b) {
        return m_bodies.at(a).depth < m_bodies.at(b).depth;
    });
}

void PhysicsBodies::beforeStep(float stepSeconds)
{
    for (const NodeId owner : m_order) {
        Body& body = m_bodies.at(owner);
        const auto* node = dynamic_cast<const Node3D*>(m_scene.find(owner));
        if (!node) {
            continue;
        }
        const BodyPose pose = bodyPoseOf(node->worldMatrix());
        const bool moved = pose != body.remembered;
        body.remembered = pose;
        if (body.motion == JPH::EMotionType::Kinematic) {
            // Без новой цели кинематическое тело шло бы дальше с прежней скоростью.
            if (moved || m_interface.IsActive(body.id)) {
                m_interface.MoveKinematic(body.id, toJolt(pose.position), toJolt(pose.rotation), stepSeconds);
            }
            continue;
        }
        if (moved) {
            m_interface.SetPositionAndRotation(
                body.id,
                toJolt(pose.position),
                toJolt(pose.rotation),
                JPH::EActivation::Activate
            );
            body.previous = pose;
            body.current = pose;
            body.lastTarget = pose;
        }
    }
}

void PhysicsBodies::writePose(Node& node, Body& body, const BodyPose& pose)
{
    auto& node3D = static_cast<Node3D&>(node);
    node3D.setWorldMatrix(worldWithPose(node3D.worldMatrix(), pose));
    body.remembered = bodyPoseOf(node3D.worldMatrix());
}

void PhysicsBodies::afterStep()
{
    // Родители раньше детей: мировое положение ребёнка пишется от нового родителя.
    for (const NodeId owner : m_order) {
        Body& body = m_bodies.at(owner);
        if (body.motion != JPH::EMotionType::Dynamic) {
            continue;
        }
        body.previous = body.current;
        if (m_interface.IsActive(body.id)) {
            JPH::RVec3 position;
            JPH::Quat rotation;
            m_interface.GetPositionAndRotation(body.id, position, rotation);
            body.current = {fromJolt(position), glm::normalize(fromJolt(rotation))};
        }
        Node* node = m_scene.find(owner);
        if (node && !body.interpolation && body.current != body.lastTarget) {
            writePose(*node, body, body.current);
            body.lastTarget = body.current;
        }
    }
}

void PhysicsBodies::interpolate(float fraction)
{
    for (const NodeId owner : m_order) {
        Body& body = m_bodies.at(owner);
        if (body.motion != JPH::EMotionType::Dynamic || !body.interpolation) {
            continue;
        }
        const BodyPose target = blendPoses(body.previous, body.current, fraction);
        Node* node = m_scene.find(owner);
        if (node && target != body.lastTarget) {
            writePose(*node, body, target);
            body.lastTarget = target;
        }
    }
}

JPH::BodyID PhysicsBodies::bodyOf(NodeId owner) const
{
    const auto it = m_bodies.find(owner);

    return it != m_bodies.end() ? it->second.id : JPH::BodyID();
}

} // namespace yqlora
