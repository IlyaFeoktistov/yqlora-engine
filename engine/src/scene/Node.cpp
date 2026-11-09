#include "yqlora/scene/Node.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/reflection/ComponentRules.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/ObjectId.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

namespace yqlora {

Node::Node(std::string name)
    : m_id(newObjectId())
    , m_name(std::move(name))
{
}

Node::~Node() = default;

void Node::setId(NodeId id)
{
    assert(id != 0 && "setId: 0 means no node");
    const NodeId previous = std::exchange(m_id, id);
    if (m_scene) {
        m_scene->nodeIdChanged(*this, previous);
    }
}

Node& Node::addChild(std::unique_ptr<Node> child)
{
    return insertChild(m_children.size(), std::move(child));
}

Node& Node::insertChild(std::size_t index, std::unique_ptr<Node> child)
{
    assert(child && "insertChild: child is null");
    assert(!child->m_parent && "insertChild: node already has a parent, call removeChild first");
    assert(index <= m_children.size() && "insertChild: index is out of range");

    child->m_parent = this;
    Node& ref = *child;
    m_children.insert(m_children.begin() + static_cast<std::ptrdiff_t>(index), std::move(child));
    ref.updateActiveInHierarchy();
    ref.onParentChanged();
    if (m_scene) {
        ref.enterScene(*m_scene);
    }

    return ref;
}

std::unique_ptr<Node> Node::removeChild(Node& child)
{
    const auto it = std::ranges::find_if(m_children, [&](const auto& c) {
        return c.get() == &child;
    });
    if (it == m_children.end()) {
        return nullptr;
    }
    assert(!(m_scene && m_scene->inFrame()) && "removeChild: during a frame use Scene::destroy or Scene::setParent");
    // Узел уходит из дерева посреди игры: он и его дети останавливаются.
    child.stop();
    if (child.m_scene) {
        child.exitScene();
    }
    std::unique_ptr<Node> result = std::move(*it);
    m_children.erase(it);
    result->m_parent = nullptr;
    result->updateActiveInHierarchy();
    result->onParentChanged();

    return result;
}

Component& Node::addComponent(std::unique_ptr<Component> component)
{
    if (m_scene) {
        addRequiredComponents(*component);
    }

    return insertComponent(m_components.size(), std::move(component));
}

void Node::addRequiredComponents(const Component& component)
{
    const TypeRegistry& registry = m_scene->services().registry();
    const TypeInfo* type = registry.find(component.typeName());
    if (!type) {
        // Тип не зарегистрирован: его правил никто не знает.
        return;
    }
    const auto missing = canAttach(registry, *type, *this) ? missingRequirements(registry, *type, *this) : std::nullopt;
    if (!missing) {
        log::error("Component '{}' does not fit node '{}' of type '{}'", type->name(), name(), typeName());
        assert(false && "addComponent: the component rules are broken, see ComponentRules.hpp");

        return;
    }
    for (const TypeInfo* required : *missing) {
        insertComponent(m_components.size(), required->createComponent());
    }
}

Component& Node::insertComponent(std::size_t index, std::unique_ptr<Component> component)
{
    assert(component && "insertComponent: component is null");
    assert(!component->m_owner && "insertComponent: component already has an owner");
    assert(index <= m_components.size() && "insertComponent: index is out of range");

    component->m_owner = this;
    Component& ref = *component;
    m_components.insert(m_components.begin() + static_cast<std::ptrdiff_t>(index), std::move(component));
    if (m_scene) {
        m_scene->enterComponent(ref);
        ref.onEnterScene();
    }

    return ref;
}

Component& Node::addBuiltinComponent(std::unique_ptr<Component> component)
{
    assert(component && "addBuiltinComponent: component is null");
    component->m_builtin = true;

    return addComponent(std::move(component));
}

bool Node::canRemoveComponent(const Component& component) const
{
    return component.m_owner == this && !component.m_builtin;
}

std::unique_ptr<Component> Node::removeComponent(Component& component)
{
    if (!canRemoveComponent(component)) {
        return nullptr;
    }
    const auto it = std::ranges::find_if(m_components, [&](const auto& c) {
        return c.get() == &component;
    });
    if (it == m_components.end()) {
        return nullptr;
    }
    if (m_scene) {
        m_scene->stopComponent(component);
        component.onExitScene();
        m_scene->exitComponent(component);
    }
    std::unique_ptr<Component> result = std::move(*it);
    m_components.erase(it);
    result->m_owner = nullptr;

    return result;
}

Node* Node::findChild(std::string_view name) const
{
    const auto it = std::ranges::find_if(m_children, [&](const auto& c) {
        return c->name() == name;
    });

    return it != m_children.end() ? it->get() : nullptr;
}

void Node::enterScene(Scene& scene)
{
    m_scene = &scene;
    scene.enterNode(*this);
    for (const auto& component : m_components) {
        scene.enterComponent(*component);
        component->onEnterScene();
    }
    onEnterScene();
    for (std::size_t i = 0; i < m_children.size(); ++i) {
        m_children[i]->enterScene(scene);
    }
}

void Node::exitScene()
{
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        (*it)->exitScene();
    }
    onExitScene();
    for (const auto& component : m_components) {
        component->onExitScene();
        m_scene->exitComponent(*component);
    }
    m_scene->exitNode(*this);
    m_scene = nullptr;
}

void Node::stop()
{
    if (!m_scene) {
        // Вне сцены ничего не запускается.
        return;
    }
    for (const auto& child : m_children) {
        child->stop();
    }
    for (const auto& component : m_components) {
        m_scene->stopComponent(*component);
    }
}

void Node::moveTo(Node& parent, KeepWorld keepWorld)
{
    assert(m_parent && m_scene && parent.m_scene == m_scene && "moveTo: both nodes must be in the same scene");
    const std::function<void()> restoreWorld = keepWorld == KeepWorld::Yes ? captureWorldPosition() : nullptr;
    auto& siblings = m_parent->m_children;
    const auto it = std::ranges::find_if(siblings, [&](const auto& c) {
        return c.get() == this;
    });
    std::unique_ptr<Node> self = std::move(*it);
    siblings.erase(it);
    m_parent = &parent;
    parent.m_children.push_back(std::move(self));
    updateActiveInHierarchy();
    onParentChanged();
    if (restoreWorld) {
        restoreWorld();
    }
}

void Node::setLayer(int layer)
{
    m_layer = std::clamp(layer, 0, kLayerCount - 1);
}

void Node::setGroups(std::vector<std::string> groups)
{
    std::vector<std::string> previous = std::exchange(m_groups, std::move(groups));
    if (m_scene) {
        m_scene->nodeGroupsChanged(*this, previous);
    }
}

void Node::addToGroup(std::string_view group)
{
    if (!group.empty() && !isInGroup(group)) {
        std::vector<std::string> groups = m_groups;
        groups.emplace_back(group);
        setGroups(std::move(groups));
    }
}

void Node::removeFromGroup(std::string_view group)
{
    if (isInGroup(group)) {
        std::vector<std::string> groups = m_groups;
        std::erase(groups, group);
        setGroups(std::move(groups));
    }
}

bool Node::isInGroup(std::string_view group) const
{
    return std::ranges::find(m_groups, group) != m_groups.end();
}

Node* Node::find(std::string_view path) const
{
    // Путь только читает дерево; узел отдаётся для изменения, как findChild.
    Node* node = const_cast<Node*>(this);
    std::size_t start = 0;
    while (node && start <= path.size()) {
        const std::size_t end = std::min(path.find('/', start), path.size());
        const std::string_view part = path.substr(start, end - start);
        if (part == "..") {
            node = node->m_parent;
        } else if (!part.empty() && part != ".") {
            node = node->findChild(part);
        }
        start = end + 1;
    }

    return node;
}

void Node::setActive(bool active)
{
    m_active = active;
    updateActiveInHierarchy();
}

void Node::updateActiveInHierarchy()
{
    const bool active = m_active && (!m_parent || m_parent->m_activeInHierarchy);
    if (active == m_activeInHierarchy) {
        return;
    }
    m_activeInHierarchy = active;
    // По индексу: onDisable может поменять списки узла.
    for (std::size_t i = 0; m_scene && i < m_components.size(); ++i) {
        Component& component = *m_components[i];
        if (!component.enabled()) {
            continue;
        }
        if (active) {
            m_scene->activate(component);
        } else {
            m_scene->deactivate(component);
        }
    }
    for (std::size_t i = 0; i < m_children.size(); ++i) {
        m_children[i]->updateActiveInHierarchy();
    }
}

} // namespace yqlora
