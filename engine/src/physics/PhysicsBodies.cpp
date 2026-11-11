// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsBodies.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/physics/Collider.hpp"
#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/JoltShapes.hpp"
#include "yqlora/physics/MeshShapeCache.hpp"
#include "yqlora/physics/PhysicsLayers.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/Transform3D.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <algorithm>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

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

// Оси, которые тело может менять: все, кроме запрещённых.
JPH::EAllowedDOFs allowedDofs(const RigidBody& body)
{
    using Dofs = std::underlying_type_t<JPH::EAllowedDOFs>;
    const auto bit = [](JPH::EAllowedDOFs dof) {
        return static_cast<Dofs>(dof);
    };
    auto allowed = bit(JPH::EAllowedDOFs::All);
    const AxisLocks& position = body.lockPosition();
    const AxisLocks& rotation = body.lockRotation();
    const std::pair<bool, JPH::EAllowedDOFs> locks[] = {
        {position.x, JPH::EAllowedDOFs::TranslationX},
        {position.y, JPH::EAllowedDOFs::TranslationY},
        {position.z, JPH::EAllowedDOFs::TranslationZ},
        {rotation.x, JPH::EAllowedDOFs::RotationX},
        {rotation.y, JPH::EAllowedDOFs::RotationY},
        {rotation.z, JPH::EAllowedDOFs::RotationZ},
    };
    for (const auto& [locked, dof] : locks) {
        if (locked) {
            allowed = static_cast<Dofs>(allowed & ~bit(dof));
        }
    }

    return static_cast<JPH::EAllowedDOFs>(allowed);
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

PhysicsBodies::PhysicsBodies(Scene& scene, JPH::BodyInterface& bodies, MeshShapeCache& meshShapes)
    : m_scene(scene)
    , m_interface(bodies)
    , m_meshShapes(meshShapes)
    , m_resources(scene.services().resources())
{
}

bool PhysicsBodies::firstWarning(std::uint64_t id)
{
    return m_warned.insert(id).second;
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
    // Форма другая: дочерний коллайдер сдвинули относительно узла тела,
    // сменился масштаб узла тела, перевыгрузили меш.
    for (auto& [owner, body] : m_bodies) {
        if (m_dirtyOwners.contains(owner)) {
            continue;
        }
        const auto* node = dynamic_cast<const Node3D*>(m_scene.find(owner));
        if (!node) {
            continue;
        }
        const bool moved = !body.childRevisions.empty() && childRevisionsOf(*node) != body.childRevisions;
        const bool scaled = axisScaleOf(node->worldMatrix()) != body.scale;
        const bool reloaded = std::ranges::any_of(body.meshRevisions, [&](const auto& mesh) {
            return MeshShapeCache::revisionOf(m_resources, mesh.first) != mesh.second;
        });
        if (moved || scaled || reloaded) {
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
    if (!ownsColliders) {
        remove(owner);

        return;
    }

    JPH::EMotionType motion = rigidBody ? joltMotion(rigidBody->motion()) : JPH::EMotionType::Static;
    JPH::EAllowedDOFs allowed = rigidBody ? allowedDofs(*rigidBody) : JPH::EAllowedDOFs::All;
    if (motion == JPH::EMotionType::Dynamic && allowed == JPH::EAllowedDOFs::None) {
        if (firstWarning(owner)) {
            log::warn("Physics: body '{}' has every axis locked and stays static", node->name());
        }
        motion = JPH::EMotionType::Static;
        allowed = JPH::EAllowedDOFs::All;
    }

    // Фигуры - в осях тела без масштаба: масштаб узлов уходит в их размеры.
    const Mat4 ownerWorld = node3D->worldMatrix();
    const BodyPose pose = bodyPoseOf(ownerWorld);
    const Mat4 toBody = glm::inverse(glm::translate(Mat4(1.0f), pose.position) * glm::mat4_cast(pose.rotation));
    std::vector<PlacedShape> shapes;
    std::vector<std::pair<ResourceId, std::uint64_t>> meshes;
    forEachBodyCollider(*node, rigidBody != nullptr, [&](const Collider& collider, const Node& colliderNode) {
        const Mat4 local = toBody * static_cast<const Node3D&>(colliderNode).worldMatrix()
                           * glm::translate(Mat4(1.0f), collider.center()) * glm::mat4_cast(collider.rotation());
        const BodyPose place = bodyPoseOf(local);
        const Vec3 scale = axisScaleOf(local);
        ColliderShape shape = collider.shape();
        PlacedShape placed{.shape = {}, .position = place.position, .rotation = place.rotation, .scale = Vec3(1.0f)};
        if (auto* mesh = std::get_if<MeshShape>(&shape)) {
            // Треугольники Jolt сталкивает только у неподвижных и кинематических тел.
            if (!mesh->convex && motion == JPH::EMotionType::Dynamic) {
                if (firstWarning(collider.id())) {
                    log::warn(
                        "Physics: a mesh collider on '{}' moves with a dynamic body, its convex hull is used",
                        colliderNode.name()
                    );
                }
                mesh->convex = true;
            }
            meshes.emplace_back(mesh->mesh, MeshShapeCache::revisionOf(m_resources, mesh->mesh));
            placed.scale = scale;
        } else {
            bool uniformLost = false;
            shape = scaledShape(shape, scale, uniformLost);
            if (uniformLost && firstWarning(collider.id())) {
                log::warn(
                    "Physics: collider '{}' on '{}' cannot follow a non-uniform scale, it takes the largest axis",
                    collider.typeName(),
                    colliderNode.name()
                );
            }
        }
        placed.shape = shape;
        shapes.push_back(std::move(placed));
        m_colliderOwners[collider.id()] = owner;
    });
    if (!rigidBody && shapes.empty()) {
        remove(owner);

        return;
    }

    // Тело делается заново, а скорость переходит к новому. Новое не спит:
    // изменённая форма могла упереться в соседей.
    JPH::Vec3 linearVelocity = JPH::Vec3::sZero();
    JPH::Vec3 angularVelocity = JPH::Vec3::sZero();
    bool existed = false;
    if (const auto it = m_bodies.find(owner); it != m_bodies.end()) {
        linearVelocity = m_interface.GetLinearVelocity(it->second.id);
        angularVelocity = m_interface.GetAngularVelocity(it->second.id);
        existed = true;
        remove(owner);
    }

    const PhysicsObjectLayer layer = physicsObjectLayer(node->layer(), motion != JPH::EMotionType::Static);
    const MeshShapeSource meshSource = [&](ResourceId mesh, bool convex) {
        return m_meshShapes.shape(m_resources, mesh, convex);
    };
    JPH::BodyCreationSettings
        settings(joltBodyShape(shapes, meshSource), toJolt(pose.position), toJolt(pose.rotation), motion, layer);
    settings.mUserData = owner;
    if (rigidBody && motion != JPH::EMotionType::Static) {
        settings.mLinearDamping = rigidBody->linearDamping();
        settings.mAngularDamping = rigidBody->angularDamping();
        settings.mGravityFactor = rigidBody->gravityScale();
        settings.mAllowedDOFs = allowed;
        if (rigidBody->continuousCollision()) {
            settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
        }
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
    const bool asleep = !existed && rigidBody && rigidBody->startAsleep();
    const JPH::EActivation activation = motion == JPH::EMotionType::Static || asleep ? JPH::EActivation::DontActivate
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
        .scale = axisScaleOf(ownerWorld),
        .meshRevisions = std::move(meshes),
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
