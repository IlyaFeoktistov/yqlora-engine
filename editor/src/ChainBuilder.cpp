#include "yqlora/editor/ChainBuilder.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/SceneEditing.hpp"
#include "yqlora/physics/CapsuleCollider.hpp"
#include "yqlora/physics/Joint.hpp"
#include "yqlora/physics/PhysicsWorld.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/resource/BuiltinResources.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/Transform3D.hpp"

#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace yqlora::editor {

namespace {

// Встроенная капсула - радиус 0.5 и высота 2: масштаб узла меша под звено.
constexpr float kBuiltinCapsuleRadius = 0.5f;
constexpr float kBuiltinCapsuleHeight = 2.0f;

// Пределы звеньев на качании и кручении: цепь гнётся, но не складывается.
constexpr float kLinkSwingDegrees = 45.0f;
constexpr float kLinkTwistDegrees = 10.0f;

// Точка касания для поиска тела под концом цепи, м.
constexpr float kAnchorProbeRadius = 0.01f;

const Vec3 kLinkAxis(0.0f, 1.0f, 0.0f);

void addJoint(Node3D& link, const ChainSettings& settings, const Vec3& anchor, NodeId connected)
{
    Joint* joint = nullptr;
    if (settings.joint == ChainLinkJoint::SwingTwist) {
        auto& swingTwist = link.addComponent<SwingTwistJoint>();
        swingTwist.setNormalHalfConeAngle(kLinkSwingDegrees);
        swingTwist.setPlaneHalfConeAngle(kLinkSwingDegrees);
        swingTwist.setTwistMin(-kLinkTwistDegrees);
        swingTwist.setTwistMax(kLinkTwistDegrees);
        joint = &swingTwist;
    } else {
        joint = &link.addComponent<PointJoint>();
    }
    joint->setAnchor(anchor);
    joint->setConnectedBody(NodeRef{connected});
}

} // namespace

std::unique_ptr<Node> buildChain(const ChainSettings& settings, const ChainAnchors& anchors)
{
    const Vec3 path = settings.start - settings.end;
    const float length = glm::length(path);
    if (length < kMinChainLinkSize) {
        return nullptr;
    }
    const float wanted = std::max(settings.spacing, kMinChainLinkSize);
    const int count = std::max(1, static_cast<int>(std::lround(length / wanted)));
    const float spacing = length / static_cast<float>(count);
    const float radius = std::clamp(settings.radius, kMinChainLinkSize, spacing * 0.5f);
    // Ось Y звена смотрит к началу цепи: там его точка крепления.
    const Vec3 towardStart = path / length;
    const Quat rotation = glm::rotation(kLinkAxis, towardStart);
    const Vec3 topAnchor(0.0f, spacing * 0.5f, 0.0f);

    auto chain = std::make_unique<Node3D>("Chain");
    chain->transform().setPosition(settings.start);
    NodeId previous = settings.attachStart ? anchors.start : 0;
    bool firstLink = true;
    Node3D* last = nullptr;
    for (int i = 0; i < count; ++i) {
        auto& link = chain->addChild<Node3D>("Link " + std::to_string(i + 1));
        const float along = spacing * (static_cast<float>(i) + 0.5f);
        link.transform().setPosition(-towardStart * along);
        link.transform().setRotation(rotation);
        link.addComponent<RigidBody>().setMass(settings.linkMass);
        auto& collider = link.addComponent<CapsuleCollider>();
        collider.setRadius(radius);
        collider.setHeight(spacing);
        auto& mesh = link.addChild<Node3D>("Mesh");
        mesh.transform().setScale(
            {radius / kBuiltinCapsuleRadius, spacing / kBuiltinCapsuleHeight, radius / kBuiltinCapsuleRadius}
        );
        mesh.addComponent<MeshRenderer>().setMesh(ResourceRef{builtin::kCapsuleMesh});
        if (!firstLink || settings.attachStart) {
            addJoint(link, settings, topAnchor, previous);
        }
        firstLink = false;
        previous = link.id();
        last = &link;
    }
    if (settings.attachEnd) {
        addJoint(*last, settings, -topAnchor, anchors.end);
    }

    return chain;
}

NodeId bodyNodeAt(Scene& scene, const Vec3& point)
{
    PhysicsWorld* physics = scene.system<PhysicsWorld>();
    if (!physics) {
        return 0;
    }
    for (const ComponentRef<Collider>& collider : physics->overlapSphere(point, kAnchorProbeRadius)) {
        Node* node = scene.find(collider.node);
        // Тело собирает ближайший RigidBody выше; без него - узел коллайдера.
        for (Node* up = node; up; up = up->parent()) {
            if (up->getComponent<RigidBody>()) {
                return up->id();
            }
        }
        if (node) {
            return node->id();
        }
    }

    return 0;
}

Node* createChain(EditorSession& session, const ChainSettings& settings)
{
    Scene& scene = session.scene();
    scene.updateTransforms();
    const ChainAnchors anchors{
        .start = settings.attachStart ? bodyNodeAt(scene, settings.start) : 0,
        .end = settings.attachEnd ? bodyNodeAt(scene, settings.end) : 0,
    };
    std::unique_ptr<Node> chain = buildChain(settings, anchors);
    if (!chain) {
        return nullptr;
    }
    // Звенья заданы в осях мира: корень цепи - в точке мира start.
    if (const auto* parent = dynamic_cast<const Node3D*>(scene.root())) {
        static_cast<Node3D&>(*chain).transform().setPosition(parent->toLocal(settings.start));
    }

    return session.editing().addBranch(std::move(chain), nullptr);
}

} // namespace yqlora::editor
