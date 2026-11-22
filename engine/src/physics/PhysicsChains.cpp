// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsChains.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/physics/CharacterController.hpp"
#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/PhysicsBodies.hpp"
#include "yqlora/physics/PhysicsChain.hpp"
#include "yqlora/physics/PhysicsLayers.hpp"
#include "yqlora/physics/PhysicsMaterialCache.hpp"
#include "yqlora/physics/PhysicsSettings.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/Scene.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace yqlora {

namespace {

// Звенья лежат от узла назад: вперёд у узла - -Z (Node3D::worldForward).
const Vec3 kChainAxis(0.0f, 0.0f, 1.0f);
const Vec3 kSideAxis(0.0f, 1.0f, 0.0f);
const Vec3 kUpAxis(1.0f, 0.0f, 0.0f);

Node3D* nodeOf(const PhysicsChain& chain)
{
    return dynamic_cast<Node3D*>(chain.owner());
}

// Поза b в осях позы a - в мире.
BodyPose compose(const BodyPose& a, const BodyPose& b)
{
    return {a.position + a.rotation * b.position, glm::normalize(a.rotation * b.rotation)};
}

BodyPose inverse(const BodyPose& pose)
{
    const Quat rotation = glm::inverse(pose.rotation);

    return {rotation * -pose.position, rotation};
}

// Капсула звена вдоль оси цепи (капсула Jolt - вдоль Y); звено короче
// двух радиусов - шар.
JPH::RefConst<JPH::Shape> linkShape(float radius, float segment, const JPH::PhysicsMaterial* material)
{
    const float halfLine = std::max(segment * 0.5f - radius, 0.0f);
    const JPH::RefConst<JPH::Shape> core = halfLine > 0.0f
                                               ? JPH::RefConst<JPH::Shape>(
                                                     new JPH::CapsuleShape(halfLine, radius, material)
                                                 )
                                               : JPH::RefConst<JPH::Shape>(new JPH::SphereShape(radius, material));

    return new JPH::RotatedTranslatedShape(
        JPH::Vec3::sZero(),
        JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::JPH_PI * 0.5f),
        core
    );
}

// Поворот следующего звена относительно предыдущего для изгиба (вбок, вверх) в градусах.
Quat bendRotation(const Vec2& bend)
{
    return glm::angleAxis(glm::radians(bend.x), kSideAxis) * glm::angleAxis(glm::radians(-bend.y), kUpAxis);
}

// Изгиб по повороту следующего звена относительно предыдущего: куда
// смотрит его ось - вбок и вверх.
Vec2 bendOf(const Quat& relative)
{
    const Vec3 axis = relative * kChainAxis;

    return {
        glm::degrees(std::atan2(axis.x, axis.z)),
        glm::degrees(std::atan2(axis.y, std::hypot(axis.x, axis.z))),
    };
}

} // namespace

PhysicsChains::PhysicsChains(
    Scene& scene,
    JPH::PhysicsSystem& system,
    PhysicsBodies& bodies,
    PhysicsMaterialCache& materials
)
    : m_scene(scene)
    , m_system(system)
    , m_bodies(bodies)
    , m_materials(materials)
    , m_resources(scene.services().resources())
{
}

PhysicsChains::~PhysicsChains()
{
    for (auto& [id, entry] : m_chains) {
        destroy(entry);
    }
}

void PhysicsChains::changed(PhysicsChain& chain)
{
    Entry& entry = m_chains[chain.id()];
    entry.chain = &chain;
    entry.dirty = true;
}

void PhysicsChains::tuned(PhysicsChain& chain)
{
    const auto found = m_chains.find(chain.id());
    if (found != m_chains.end()) {
        found->second.tune = true;
    }
}

void PhysicsChains::left(const PhysicsChain& chain)
{
    const auto found = m_chains.find(chain.id());
    if (found == m_chains.end()) {
        return;
    }
    destroy(found->second);
    m_chains.erase(found);
}

void PhysicsChains::build(Entry& entry)
{
    entry.dirty = false;
    const PhysicsChain& chain = *entry.chain;
    const Node3D& node = *nodeOf(chain);
    const BodyPose nodePose = bodyPoseOf(node.worldMatrix());
    const int count = chain.segments();
    const float segment = chain.length() / static_cast<float>(count);
    const float linkMass = chain.mass() / static_cast<float>(count);
    const PhysicsSettings& physics = physicsSettingsOf(m_scene.services());
    const JPH::RefConst<JPH::Shape> shape = linkShape(
        chain.radius(),
        segment,
        m_materials.material(m_resources, physics.materialFor(chain.material()))
    );
    // Соседи перекрываются в шарнире и друг с другом не сталкиваются;
    // дальние звенья сталкиваются, поэтому цепь не проходит сама сквозь себя.
    const JPH::Ref<JPH::GroupFilterTable> neighbours = new JPH::GroupFilterTable(static_cast<JPH::uint>(count));
    for (int i = 0; i + 1 < count; ++i) {
        neighbours->DisableCollision(static_cast<JPH::uint>(i), static_cast<JPH::uint>(i + 1));
    }
    const JPH::CollisionGroup::GroupID group = m_nextGroup++;
    const PhysicsObjectLayer layer = physicsObjectLayer(node.layer(), true);
    JPH::BodyInterface& bodies = m_system.GetBodyInterface();

    std::vector<BodyPose> worlds;
    for (int i = 0; i < count; ++i) {
        const BodyPose rest{kChainAxis * (segment * (static_cast<float>(i) + 0.5f)), Quat(1.0f, 0.0f, 0.0f, 0.0f)};
        const BodyPose world = compose(nodePose, rest);
        JPH::BodyCreationSettings
            settings(shape, toJolt(world.position), toJolt(world.rotation), JPH::EMotionType::Dynamic, layer);
        settings.mUserData = node.id();
        settings.mCollisionGroup = JPH::CollisionGroup(neighbours, group, static_cast<JPH::uint>(i));
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = linkMass;
        const JPH::BodyID id = bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
        if (id.IsInvalid()) {
            log::error("Physics: no room for the links of chain '{}', the world is full", node.name());
            destroy(entry);

            return;
        }
        m_bodies.addChainLink(id, node.id(), chain.id(), i);
        entry.links.push_back(
            {.id = id, .rest = rest, .previous = world, .current = world, .groundNormal = Vec3(0.0f), .ground = {}}
        );
        worlds.push_back(world);
    }

    const Vec3 axis = nodePose.rotation * kChainAxis;
    const Vec3 plane = nodePose.rotation * kSideAxis;
    for (int i = 0; i + 1 < count; ++i) {
        const Vec3 point = nodePose.position + axis * (segment * static_cast<float>(i + 1));
        JPH::SwingTwistConstraintSettings settings;
        settings.mPosition1 = JPH::RVec3(toJolt(point));
        settings.mPosition2 = settings.mPosition1;
        settings.mTwistAxis1 = toJolt(axis);
        settings.mTwistAxis2 = settings.mTwistAxis1;
        settings.mPlaneAxis1 = toJolt(plane);
        settings.mPlaneAxis2 = settings.mPlaneAxis1;
        settings.mNormalHalfConeAngle = glm::radians(chain.bendLimit());
        settings.mPlaneHalfConeAngle = settings.mNormalHalfConeAngle;
        settings.mTwistMinAngle = -glm::radians(chain.twistLimit());
        settings.mTwistMaxAngle = glm::radians(chain.twistLimit());
        const auto index = static_cast<std::size_t>(i);
        JPH::TwoBodyConstraint* constraint = bodies.CreateConstraint(
            &settings,
            entry.links[index].id,
            entry.links[index + 1].id
        );
        if (!constraint) {
            continue;
        }
        m_system.AddConstraint(constraint);
        entry.joints.emplace_back(static_cast<JPH::SwingTwistConstraint*>(constraint));
    }

    // Узел цепи - в той же позе от головы, что сейчас.
    entry.headOffset = inverse(entry.links.front().rest);
    // Узлы звеньев - в той позе от своего звена, что сейчас. Цепь,
    // построенная заново в идущей сцене, звенья распрямляет, поэтому позы
    // тех же узлов берутся прежние.
    std::vector<Follower> followers;
    for (const auto& child : node.children()) {
        if (static_cast<int>(followers.size()) == count) {
            break;
        }
        const auto* child3D = dynamic_cast<const Node3D*>(child.get());
        if (!child3D) {
            continue;
        }
        const std::size_t index = followers.size();
        const bool known = m_scene.playing() && index < entry.followers.size()
                           && entry.followers[index].node == child3D->id();
        const BodyPose offset = known ? entry.followers[index].offset
                                      : compose(inverse(worlds[index]), bodyPoseOf(child3D->worldMatrix()));
        followers.push_back({child3D->id(), offset});
    }
    entry.followers = std::move(followers);
    entry.remembered = nodePose;
    entry.layer = node.layer();
    configure(entry);
}

void PhysicsChains::destroy(Entry& entry)
{
    for (const auto& joint : entry.joints) {
        m_system.RemoveConstraint(joint);
    }
    entry.joints.clear();
    JPH::BodyInterface& bodies = m_system.GetBodyInterface();
    for (const Link& link : entry.links) {
        m_bodies.removeChainLink(link.id);
        bodies.RemoveBody(link.id);
        bodies.DestroyBody(link.id);
    }
    entry.links.clear();
}

void PhysicsChains::configure(Entry& entry)
{
    entry.tune = false;
    const PhysicsChain& chain = *entry.chain;
    // Пружина нулевой жёсткости у Jolt жёсткая: без жёсткости мышц нет.
    const bool working = !chain.relaxed() && chain.maxTorque() > 0.0f && chain.stiffness() > 0.0f;
    const JPH::EMotorState state = working ? JPH::EMotorState::Position : JPH::EMotorState::Off;
    const JPH::SpringSettings spring(JPH::ESpringMode::StiffnessAndDamping, chain.stiffness(), chain.damping());
    JPH::BodyInterface& bodies = m_system.GetBodyInterface();
    for (std::size_t i = 0; i < entry.joints.size(); ++i) {
        JPH::SwingTwistConstraint& joint = *entry.joints[i];
        for (JPH::MotorSettings* motor : {&joint.GetSwingMotorSettings(), &joint.GetTwistMotorSettings()}) {
            motor->mSpringSettings = spring;
            motor->SetTorqueLimit(chain.maxTorque());
        }
        joint.SetSwingMotorState(state);
        joint.SetTwistMotorState(state);
        // Кручение мышцы держат в нуле: цепь гнётся, но не закручивается.
        joint.SetTargetOrientationBS(toJolt(bendRotation(chain.targetBend(static_cast<int>(i)))));
        bodies.ActivateConstraint(&joint);
    }
}

BodyPose PhysicsChains::linkPose(const Link& link) const
{
    JPH::RVec3 position;
    JPH::Quat rotation;
    m_system.GetBodyInterface().GetPositionAndRotation(link.id, position, rotation);

    return {fromJolt(position), glm::normalize(fromJolt(rotation))};
}

void PhysicsChains::moveTo(Entry& entry, const BodyPose& pose)
{
    const BodyPose head = compose(linkPose(entry.links.front()), entry.headOffset);
    const BodyPose shift = compose(pose, inverse(head));
    JPH::BodyInterface& bodies = m_system.GetBodyInterface();
    for (Link& link : entry.links) {
        const BodyPose moved = compose(shift, linkPose(link));
        bodies.SetPositionAndRotation(
            link.id,
            toJolt(moved.position),
            toJolt(moved.rotation),
            JPH::EActivation::Activate
        );
        // Скорость поворачивается вместе с цепью.
        bodies.SetLinearAndAngularVelocity(
            link.id,
            toJolt(shift.rotation * fromJolt(bodies.GetLinearVelocity(link.id))),
            toJolt(shift.rotation * fromJolt(bodies.GetAngularVelocity(link.id)))
        );
        link.previous = moved;
        link.current = moved;
    }
    entry.remembered = pose;
}

void PhysicsChains::sync()
{
    for (auto& [id, entry] : m_chains) {
        const PhysicsChain& chain = *entry.chain;
        const Node3D* node = nodeOf(chain);
        if (!node || !chain.activeAndEnabled()) {
            destroy(entry);
            continue;
        }
        const bool rival = node->getComponent<RigidBody>() || node->getComponent<CharacterController>();
        if (rival && m_warnings.first(id)) {
            log::warn(
                "Physics: node '{}' has a PhysicsChain and a RigidBody or CharacterController; they would both move it",
                node->name()
            );
        }
        if (entry.links.empty() || entry.dirty || entry.layer != node->layer()) {
            destroy(entry);
            build(entry);
            continue;
        }
        const BodyPose pose = bodyPoseOf(node->worldMatrix());
        if (pose != entry.remembered) {
            moveTo(entry, pose);
        }
        if (entry.tune) {
            configure(entry);
        }
    }
}

void PhysicsChains::beforeStep()
{
    JPH::BodyInterface& bodies = m_system.GetBodyInterface();
    for (auto& [id, entry] : m_chains) {
        const float sideGrip = entry.chain->sideGrip();
        const float forwardGrip = entry.chain->forwardGrip();
        if (sideGrip == 0.0f && forwardGrip == 0.0f) {
            continue;
        }
        for (const Link& link : entry.links) {
            const Vec3& normal = link.groundNormal;
            if (normal == Vec3(0.0f)) {
                continue;
            }
            const Vec3 velocity = fromJolt(bodies.GetLinearVelocity(link.id));
            const Vec3 groundVelocity = link.ground.IsInvalid()
                                            ? Vec3(0.0f)
                                            : fromJolt(
                                                  bodies.GetPointVelocity(link.ground, toJolt(link.current.position))
                                              );
            // Скольжение по земле: скорость относительно неё без части по нормали.
            Vec3 sliding = velocity - groundVelocity;
            sliding -= normal * glm::dot(sliding, normal);
            Vec3 axis = link.current.rotation * kChainAxis;
            axis -= normal * glm::dot(axis, normal);
            const float axisLength = glm::length(axis);
            // Звено торчит из земли: вдоль и поперёк не различить, всё - вбок.
            const Vec3 along = axisLength > math::kEpsilon
                                   ? axis * (glm::dot(sliding, axis) / (axisLength * axisLength))
                                   : Vec3(0.0f);
            const Vec3 across = sliding - along;
            bodies.SetLinearVelocity(link.id, toJolt(velocity - across * sideGrip - along * forwardGrip));
        }
    }
}

void PhysicsChains::afterStep(const std::vector<RawContact>& contacts)
{
    JPH::BodyInterface& bodies = m_system.GetBodyInterface();
    for (auto& [id, entry] : m_chains) {
        for (Link& link : entry.links) {
            link.previous = link.current;
            if (bodies.IsActive(link.id)) {
                link.current = linkPose(link);
            }
            link.groundNormal = Vec3(0.0f);
            link.ground = JPH::BodyID();
        }
    }
    // Нормаль касания смотрит из первого тела во второе: земля толкает
    // второе по ней, а первое - против.
    const auto touch = [&](std::uint32_t self, std::uint32_t other, std::uint32_t otherPart, const Vec3& normal) {
        const auto selfLink = m_bodies.chainLinkOf(self);
        if (!selfLink) {
            return;
        }
        const auto [chainId, index] = *selfLink;
        const auto otherLink = m_bodies.chainLinkOf(other);
        if (otherLink && otherLink->first == chainId) {
            return;
        }
        const auto side = m_bodies.sideOf(other, otherPart);
        if (!side || side->trigger) {
            return;
        }
        Link& link = m_chains.at(chainId).links[static_cast<std::size_t>(index)];
        link.groundNormal += normal;
        link.ground = JPH::BodyID(other);
    };
    for (const RawContact& contact : contacts) {
        touch(contact.first, contact.second, contact.secondPart, -contact.normal);
        touch(contact.second, contact.first, contact.firstPart, contact.normal);
    }
    for (auto& [id, entry] : m_chains) {
        for (Link& link : entry.links) {
            const float length = glm::length(link.groundNormal);
            link.groundNormal = length > math::kEpsilon ? link.groundNormal / length : Vec3(0.0f);
        }
    }
}

void PhysicsChains::write(Entry& entry, const std::vector<BodyPose>& poses)
{
    Node3D& node = *nodeOf(*entry.chain);
    node.setWorldMatrix(worldWithPose(node.worldMatrix(), compose(poses.front(), entry.headOffset)));
    entry.remembered = bodyPoseOf(node.worldMatrix());
    for (std::size_t i = 0; i < entry.followers.size(); ++i) {
        auto* child = dynamic_cast<Node3D*>(m_scene.find(entry.followers[i].node));
        if (child) {
            child->setWorldMatrix(worldWithPose(child->worldMatrix(), compose(poses[i], entry.followers[i].offset)));
        }
    }
}

void PhysicsChains::interpolate(float fraction)
{
    // Звенья сцены, которая не шагает, стоят на месте узлов: писать нечего.
    if (!m_scene.playing()) {
        return;
    }
    std::vector<BodyPose> poses;
    for (auto& [id, entry] : m_chains) {
        const Node3D* node = nodeOf(*entry.chain);
        if (entry.links.empty() || !node) {
            continue;
        }
        poses.clear();
        bool moved = false;
        for (const Link& link : entry.links) {
            poses.push_back(blendPoses(link.previous, link.current, fraction));
            moved = moved || link.previous != link.current;
        }
        // Всё стоит, а узел там, куда его поставила цепь: писать нечего.
        if (moved || entry.remembered != bodyPoseOf(node->worldMatrix())) {
            write(entry, poses);
        }
    }
}

void PhysicsChains::wakeAll()
{
    JPH::BodyInterface& bodies = m_system.GetBodyInterface();
    for (const auto& [id, entry] : m_chains) {
        for (const Link& link : entry.links) {
            bodies.ActivateBody(link.id);
        }
    }
}

const PhysicsChains::Link* PhysicsChains::linkOf(const PhysicsChain& chain, int index) const
{
    const auto found = m_chains.find(chain.id());
    if (found == m_chains.end() || index < 0 || index >= static_cast<int>(found->second.links.size())) {
        return nullptr;
    }

    return &found->second.links[static_cast<std::size_t>(index)];
}

Vec2 PhysicsChains::bend(const PhysicsChain& chain, int index) const
{
    const Link* link = linkOf(chain, index);
    const Link* next = linkOf(chain, index + 1);
    if (!link || !next) {
        return Vec2(0.0f);
    }

    return bendOf(glm::inverse(linkPose(*link).rotation) * linkPose(*next).rotation);
}

Vec3 PhysicsChains::linkPosition(const PhysicsChain& chain, int index) const
{
    const Link* link = linkOf(chain, index);

    return link ? linkPose(*link).position : Vec3(0.0f);
}

Vec3 PhysicsChains::linkVelocity(const PhysicsChain& chain, int index) const
{
    const Link* link = linkOf(chain, index);

    return link ? fromJolt(m_system.GetBodyInterface().GetLinearVelocity(link->id)) : Vec3(0.0f);
}

} // namespace yqlora
