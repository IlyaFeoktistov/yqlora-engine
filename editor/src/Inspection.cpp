#include "yqlora/editor/Inspection.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/reflection/ComponentRules.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/scene/MissingComponent.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora::editor {

namespace {

std::optional<CameraRole> cameraRole(const Scene& scene, const Node& node)
{
    const auto* camera = dynamic_cast<const Camera3D*>(&node);
    const RenderSystem* render = scene.system<RenderSystem>();
    if (!camera || !render) {
        return std::nullopt;
    }
    if (!camera->activeInHierarchy()) {
        return CameraRole::Inactive;
    }

    return render->mainCamera() == camera ? CameraRole::Main : CameraRole::Secondary;
}

std::string shownTypeName(const Component& component)
{
    const auto* missing = dynamic_cast<const MissingComponent*>(&component);

    return missing ? missing->missingType() : std::string(component.typeName());
}

} // namespace

std::optional<Inspection> inspect(EditorSession& session)
{
    const TypeRegistry& registry = session.registry();
    Node* node = session.selectedNode();
    const TypeInfo* type = node ? registry.find(node->typeName()) : nullptr;
    if (!node || !type) {
        return std::nullopt;
    }

    Inspection result{
        .node = *node,
        .type = *type,
        .components = {},
        .camera = cameraRole(session.scene(), *node),
    };
    for (const auto& component : node->components()) {
        result.components.push_back({
            .component = *component,
            .type = registry.find(component->typeName()),
            .typeName = shownTypeName(*component),
            .removable = canDetach(registry, *node, *component),
            .requiredBy = dependentsOf(registry, *node, *component),
        });
    }

    return result;
}

} // namespace yqlora::editor
