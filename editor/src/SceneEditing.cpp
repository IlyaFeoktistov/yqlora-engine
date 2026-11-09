#include "yqlora/editor/SceneEditing.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/Picking.hpp"
#include "yqlora/editor/ResourceInstance.hpp"
#include "yqlora/editor/SceneCommands.hpp"
#include "yqlora/reflection/ComponentRules.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/render/ResourceUses.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/NodeTree.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace yqlora::editor {

bool SceneEditing::execute(std::unique_ptr<Command> command)
{
    return m_session.execute(std::move(command));
}

Node* SceneEditing::createNode(const TypeInfo& type, Node* parent)
{
    std::unique_ptr<Node> node = type.kind() == TypeKind::Node ? type.createNode() : nullptr;
    if (!node) {
        return nullptr;
    }
    node->setName(type.name());

    return addBranch(std::move(node), parent);
}

Node* SceneEditing::addResource(ResourceId resource, Node* parent)
{
    const auto& project = m_session.project();
    if (!project) {
        return nullptr;
    }

    return addBranch(
        instantiateResource(resource, m_session.resources(), project->resources(), m_session.registry()),
        parent
    );
}

Node* SceneEditing::addResource(ResourceId resource, Node* parent, const Vec3& worldPosition)
{
    const auto& project = m_session.project();
    if (!project) {
        return nullptr;
    }
    std::unique_ptr<Node>
        branch = instantiateResource(resource, m_session.resources(), project->resources(), m_session.registry());
    // Ветка станет ребёнком parent, а без него - корня; положение задаётся в
    // его координатах. В пустой сцене ветка сама станет корнем.
    const Node* target = parent ? parent : m_session.scene().root();
    if (auto* root = dynamic_cast<Node3D*>(branch.get())) {
        const auto* target3D = dynamic_cast<const Node3D*>(target);
        root->transform().setPosition(target3D ? target3D->toLocal(worldPosition) : worldPosition);
    }

    return addBranch(std::move(branch), parent);
}

Node* SceneEditing::addBranch(std::unique_ptr<Node> branch, Node* parent)
{
    auto command = std::make_unique<AddBranchCommand>(std::move(branch), parent ? parent->id() : 0);
    const NodeId created = command->created();
    if (created == 0 || !execute(std::move(command))) {
        return nullptr;
    }

    return m_session.findNode(created);
}

bool SceneEditing::deleteNode(NodeId id)
{
    return execute(std::make_unique<DeleteNodeCommand>(id));
}

bool SceneEditing::deleteNodes(std::span<const NodeId> ids)
{
    const auto insideAnother = [&](NodeId id) {
        const Node* node = m_session.findNode(id);

        return node && std::ranges::any_of(ids, [&](NodeId other) {
                   const Node* branch = other != id ? m_session.findNode(other) : nullptr;

                   return branch && isInBranch(*branch, *node);
               });
    };

    std::vector<std::unique_ptr<Command>> commands;
    for (const NodeId id : ids) {
        if (!insideAnother(id)) {
            commands.push_back(std::make_unique<DeleteNodeCommand>(id));
        }
    }

    return execute(std::make_unique<CompositeCommand>(std::move(commands)));
}

Component* SceneEditing::addComponent(Node& node, const TypeInfo& type)
{
    const TypeRegistry& registry = m_session.registry();
    const auto missing = canAttach(registry, type, node) ? missingRequirements(registry, type, node) : std::nullopt;
    if (!missing) {
        return nullptr;
    }

    std::vector<std::unique_ptr<Command>> commands;
    for (const TypeInfo* required : *missing) {
        commands.push_back(std::make_unique<AddComponentCommand>(node.id(), *required));
    }
    commands.push_back(std::make_unique<AddComponentCommand>(node.id(), type));
    if (!execute(std::make_unique<CompositeCommand>(std::move(commands)))) {
        return nullptr;
    }

    return node.components().back().get();
}

bool SceneEditing::removeComponent(Node& node, Component& component)
{
    const auto index = componentIndex(node, component);

    return index && canDetach(m_session.registry(), node, component)
           && execute(std::make_unique<RemoveComponentCommand>(node.id(), *index));
}

bool SceneEditing::setProperty(Node& node, const Property& property, const PropertyValue& value)
{
    return execute(
        std::make_unique<SetPropertyCommand>(
            PropertyTarget{.node = node.id(), .component = std::nullopt},
            property,
            value
        )
    );
}

bool SceneEditing::setProperty(Component& component, const Property& property, const PropertyValue& value)
{
    Node* node = component.owner();
    const auto index = node ? componentIndex(*node, component) : std::nullopt;
    if (!index) {
        return false;
    }

    return execute(
        std::make_unique<SetPropertyCommand>(PropertyTarget{.node = node->id(), .component = index}, property, value)
    );
}

bool SceneEditing::setMaterialSlot(MeshRenderer& renderer, std::size_t slot, ResourceRef material)
{
    const TypeInfo* type = m_session.registry().find(renderer.typeName());
    if (!type) {
        return false;
    }
    const auto properties = m_session.registry().properties(*type);
    const auto found = std::ranges::find(properties, kMaterialsProperty, &Property::name);
    if (found == properties.end()
        || (material.id != 0
            && !fitsResourceUse((*found)->attributes().resourceUse, material.id, m_session.resources()))) {
        return false;
    }
    std::vector<ResourceRef> materials = renderer.materials();
    if (slot < materials.size() && materials[slot] == material) {
        return false;
    }
    materials.resize(std::max(materials.size(), slot + 1));
    materials[slot] = material;
    // Отдельный шаг истории, не слитый с соседней правкой.
    m_session.finishEdit();
    const bool done = setProperty(renderer, **found, PropertyStorage<std::vector<ResourceRef>>::store(materials));
    m_session.finishEdit();

    return done;
}

bool SceneEditing::dropMaterial(const Ray& ray, ResourceRef material)
{
    const auto hit = raycastSurfaces(m_session.scene(), ray);
    Node* node = hit ? m_session.findNode(hit->node) : nullptr;
    auto* renderer = node ? node->getComponent<MeshRenderer>() : nullptr;

    return renderer && setMaterialSlot(*renderer, hit->submesh, material);
}

bool SceneEditing::setParent(Node& node, Node& parent, KeepWorld keepWorld)
{
    return execute(std::make_unique<SetParentCommand>(node.id(), parent.id(), keepWorld));
}

bool SceneEditing::setTransform(Node3D& node, const TransformValues& values)
{
    return execute(std::make_unique<SetTransformCommand>(node.id(), values));
}

bool SceneEditing::makeMainCamera(Camera3D& camera)
{
    const RenderSystem* render = m_session.scene().system<RenderSystem>();
    const TypeInfo* type = m_session.registry().find(camera.typeName());
    const auto order = render ? orderToBecomeMain(*render, camera) : std::nullopt;
    if (!type || !order) {
        return false;
    }

    const auto properties = m_session.registry().properties(*type);
    const auto property = std::ranges::find(properties, kCameraOrderProperty, &Property::name);
    if (property == properties.end()) {
        return false;
    }

    // Отдельный шаг истории, не слитый с правкой order в поле рядом.
    m_session.finishEdit();
    const bool done = setProperty(camera, **property, PropertyValue{*order});
    m_session.finishEdit();

    return done;
}

} // namespace yqlora::editor
