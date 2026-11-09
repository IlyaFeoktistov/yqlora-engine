#include "yqlora/scene/Scene.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/scene/NodeTree.hpp"
#include "yqlora/scene/TimeSettings.hpp"

#include <cassert>

namespace yqlora {

Scene::Scene(const EngineServices& services)
    : m_services(services)
{
}

Scene::~Scene()
{
    setRoot(nullptr);
}

void Scene::setRoot(std::unique_ptr<Node> root)
{
    assert((!root || !root->parent()) && "setRoot: node already has a parent");
    assert(!inFrame() && "setRoot: during a frame use Scene::destroy");
    if (m_root) {
        m_root->stop();
        m_root->exitScene();
    }
    m_root = std::move(root);
    if (m_root) {
        m_root->enterScene(*this);
    }
}

std::unique_ptr<Node> Scene::takeRoot()
{
    assert(!inFrame() && "takeRoot: during a frame use Scene::destroy");
    if (m_root) {
        m_root->stop();
        m_root->exitScene();
    }

    return std::move(m_root);
}

void Scene::destroy(Node& node)
{
    assert(node.scene() == this && "destroy: the node is not in this scene");
    if (inFrame()) {
        m_treeChanges.destroy(node);

        return;
    }
    destroyNow(node);
}

void Scene::setParent(Node& node, Node& parent, KeepWorld keepWorld)
{
    assert(node.scene() == this && parent.scene() == this && "setParent: the nodes are not in this scene");
    if (!node.parent() || isInBranch(node, parent)) {
        log::error("Scene: node '{}' cannot be moved under '{}'", node.name(), parent.name());
        assert(false && "setParent: the root cannot be moved, nor a node into its own branch");

        return;
    }
    if (inFrame()) {
        m_treeChanges.setParent(node, parent, keepWorld);

        return;
    }
    node.moveTo(parent, keepWorld);
}

void Scene::destroyNow(Node& node)
{
    if (&node == m_root.get()) {
        setRoot(nullptr);

        return;
    }
    node.parent()->removeChild(node);
}

std::span<Component* const> Scene::componentsOfType(std::string_view typeName) const
{
    return m_components.ofType(typeName);
}

void Scene::enterComponent(Component& component)
{
    m_components.add(component);
    if (component.activeAndEnabled()) {
        m_lifecycle.activate(component);
    }
}

void Scene::exitComponent(Component& component)
{
    m_lifecycle.deactivate(component);
    m_components.remove(component);
}

Node* Scene::find(const NodeRef& ref) const
{
    return ref.id != 0 ? m_nodes.find(ref.id) : nullptr;
}

void Scene::enterNode(Node& node)
{
    m_nodes.add(node);
}

void Scene::exitNode(Node& node)
{
    m_nodes.remove(node);
}

void Scene::nodeIdChanged(Node& node, NodeId previous)
{
    m_nodes.changeId(node, previous);
}

void Scene::nodeGroupsChanged(Node& node, std::span<const std::string> previous)
{
    m_nodes.changeGroups(node, previous);
}

void Scene::activate(Component& component)
{
    m_lifecycle.activate(component);
}

void Scene::deactivate(Component& component)
{
    m_lifecycle.deactivate(component);
}

void Scene::stopComponent(Component& component)
{
    m_lifecycle.stop(component);
}

void Scene::stop()
{
    if (m_root) {
        m_root->stop();
    }
}

void Scene::advance(float realDelta)
{
    m_lifecycle.beginFrame();
    m_time.setFixedStep(timeSettingsOf(m_services).fixedStep());
    m_time.beginFrame(realDelta);
    while (m_time.takeFixedStep()) {
        m_systems.fixedUpdate(m_time.fixedStep());
        m_lifecycle.runPhase(LifecyclePhase::FixedUpdate, m_time.fixedStep());
    }
    m_lifecycle.runPhase(LifecyclePhase::Update, m_time.delta());
    m_systems.update(m_time.delta());
    m_lifecycle.runPhase(LifecyclePhase::LateUpdate, m_time.delta());
    m_systems.lateUpdate(m_time.delta());
    m_lifecycle.endFrame();

    // Сначала дерево: уничтоженные уходят и из очереди включённых.
    m_treeChanges.apply(*this);
    m_lifecycle.activateJoined();
}

void Scene::updateTransforms()
{
    m_systems.updateTransforms();
}

} // namespace yqlora
