#include "yqlora/editor/BodyChainBuilder.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/SceneEditing.hpp"
#include "yqlora/physics/PhysicsChain.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/resource/BuiltinResources.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/Transform3D.hpp"

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <string>

namespace yqlora::editor {

namespace {

// Встроенная капсула - радиус 0.5 и высота 2 вдоль Y: масштаб и поворот
// узла меша под звено вдоль +Z.
constexpr float kBuiltinCapsuleRadius = 0.5f;
constexpr float kBuiltinCapsuleHeight = 2.0f;
const Vec3 kAcross(1.0f, 0.0f, 0.0f);
constexpr float kQuarterTurn = 90.0f;

} // namespace

std::unique_ptr<Node> buildBodyChain(const BodyChainSettings& settings)
{
    auto root = std::make_unique<Node3D>(settings.name);
    root->transform().setPosition(settings.position);
    auto& chain = root->addComponent<PhysicsChain>();
    chain.setSegments(settings.segments);
    chain.setLength(settings.length);
    chain.setRadius(settings.radius);
    chain.setMass(settings.mass);

    const float segment = chain.length() / static_cast<float>(chain.segments());
    const float radius = chain.radius();
    // Звено короче двух радиусов - шар: меш не короче своей толщины.
    const float height = std::max(segment, radius * 2.0f);
    const Quat alongChain = glm::angleAxis(glm::radians(kQuarterTurn), kAcross);
    for (int i = 0; i < chain.segments(); ++i) {
        auto& link = root->addChild<Node3D>("Segment " + std::to_string(i + 1));
        link.transform().setPosition({0.0f, 0.0f, segment * (static_cast<float>(i) + 0.5f)});
        link.transform().setRotation(alongChain);
        link.transform().setScale(
            {radius / kBuiltinCapsuleRadius, height / kBuiltinCapsuleHeight, radius / kBuiltinCapsuleRadius}
        );
        link.addComponent<MeshRenderer>().setMesh(ResourceRef{builtin::kCapsuleMesh});
    }

    return root;
}

Node* createBodyChain(EditorSession& session, const BodyChainSettings& settings)
{
    Scene& scene = session.scene();
    scene.updateTransforms();
    std::unique_ptr<Node> chain = buildBodyChain(settings);
    if (const auto* parent = dynamic_cast<const Node3D*>(scene.root())) {
        static_cast<Node3D&>(*chain).transform().setPosition(parent->toLocal(settings.position));
    }

    return session.editing().addBranch(std::move(chain), nullptr);
}

} // namespace yqlora::editor
