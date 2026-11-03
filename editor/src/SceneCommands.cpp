#include "yqlora/editor/SceneCommands.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/NodeTree.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace yqlora::editor {

namespace {

// "Cube2", "Cube3"...: первая копия имени получает номер 2.
constexpr int kFirstDuplicateNumber = 2;

// "Cube", "Cube2", "Cube3"... - первое свободное среди детей parent.
std::string uniqueChildName(const Node& parent, const std::string& base)
{
    if (!parent.findChild(base)) {
        return base;
    }
    for (int i = kFirstDuplicateNumber;; ++i) {
        std::string candidate = base + std::to_string(i);
        if (!parent.findChild(candidate)) {
            return candidate;
        }
    }
}

// Выделение внутри ветки сбрасывается, чтобы не осталось висячим.
DetachedBranch detach(EditorSession& session, Node& node)
{
    Scene& scene = session.scene();
    DetachedBranch branch;
    session.selection().keepNodes([&](NodeId id) {
        const Node* selectedNode = session.findNode(id);

        return !selectedNode || !isInBranch(node, *selectedNode);
    });

    Node* parent = node.parent();
    if (!parent) {
        branch.node = scene.takeRoot();

        return branch;
    }
    const auto children = parent->children();
    const auto it = std::ranges::find_if(children, [&](const auto& child) {
        return child.get() == &node;
    });
    branch.parent = parent->id();
    branch.index = static_cast<std::size_t>(it - children.begin());
    branch.node = parent->removeChild(node);

    return branch;
}

// nullptr, если места больше нет.
Node* attach(EditorSession& session, DetachedBranch& branch)
{
    Scene& scene = session.scene();
    Node* node = branch.node.get();
    if (branch.parent == 0) {
        if (scene.root()) {
            return nullptr;
        }
        scene.setRoot(std::move(branch.node));
    } else {
        Node* parent = session.findNode(branch.parent);
        if (!parent) {
            return nullptr;
        }
        parent->insertChild(std::min(branch.index, parent->children().size()), std::move(branch.node));
    }

    return node;
}

bool getProperty(EditorSession& session, const PropertyTarget& target, const Property& property, PropertyValue& value)
{
    Node* node = session.findNode(target.node);
    if (!node) {
        return false;
    }
    if (!target.component) {
        value = property.get(*node);

        return true;
    }
    const Component* component = componentAt(*node, *target.component);
    if (!component) {
        return false;
    }
    value = property.get(*component);

    return true;
}

bool setProperty(
    EditorSession& session,
    const PropertyTarget& target,
    const Property& property,
    const PropertyValue& value
)
{
    Node* node = session.findNode(target.node);
    if (!node) {
        return false;
    }
    if (!target.component) {
        return property.set(*node, value);
    }
    Component* component = componentAt(*node, *target.component);

    return component && property.set(*component, value);
}

} // namespace

AddBranchCommand::AddBranchCommand(std::unique_ptr<Node> branch, NodeId parent)
    : m_parent(parent)
{
    m_branch.node = std::move(branch);
    if (m_branch.node) {
        m_created = m_branch.node->id();
    }
}

bool AddBranchCommand::apply(EditorSession& session)
{
    if (!m_branch.node) {
        return false;
    }
    Scene& scene = session.scene();
    if (!m_placed && scene.root()) {
        Node* target = m_parent != 0 ? session.findNode(m_parent) : scene.root();
        if (!target) {
            return false;
        }
        m_branch.node->setName(uniqueChildName(*target, m_branch.node->name()));
        m_branch.parent = target->id();
        m_branch.index = target->children().size();
    }
    // Повтор после отмены вернёт узел на то же место.
    m_placed = true;

    const Selection selected = session.selection();
    if (!attach(session, m_branch)) {
        return false;
    }
    m_previousSelection = selected;
    session.selection().select(m_created);

    return true;
}

void AddBranchCommand::revert(EditorSession& session)
{
    if (Node* node = session.findNode(m_created)) {
        m_branch = detach(session, *node);
    }
    session.selection() = m_previousSelection;
}

DeleteNodeCommand::DeleteNodeCommand(NodeId node)
    : m_node(node)
{
}

bool DeleteNodeCommand::apply(EditorSession& session)
{
    Node* node = session.findNode(m_node);
    if (!node) {
        return false;
    }
    m_previousSelection = session.selection();
    m_branch = detach(session, *node);

    return true;
}

void DeleteNodeCommand::revert(EditorSession& session)
{
    if (attach(session, m_branch)) {
        session.selection() = m_previousSelection;
    }
}

CompositeCommand::CompositeCommand(std::vector<std::unique_ptr<Command>> commands)
    : m_commands(std::move(commands))
{
}

bool CompositeCommand::apply(EditorSession& session)
{
    for (std::size_t i = 0; i < m_commands.size(); ++i) {
        if (!m_commands[i]->apply(session)) {
            for (std::size_t j = i; j > 0; --j) {
                m_commands[j - 1]->revert(session);
            }

            return false;
        }
    }

    return !m_commands.empty();
}

void CompositeCommand::revert(EditorSession& session)
{
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        (*it)->revert(session);
    }
}

AddComponentCommand::AddComponentCommand(NodeId node, const TypeInfo& type)
    : m_node(node)
    , m_type(type)
{
}

bool AddComponentCommand::apply(EditorSession& session)
{
    Node* node = session.findNode(m_node);
    if (!node) {
        return false;
    }
    if (m_detached) {
        node->insertComponent(std::min(*m_index, node->components().size()), std::move(m_detached));

        return true;
    }

    std::unique_ptr<Component> component = m_type.kind() == TypeKind::Component ? m_type.createComponent() : nullptr;
    if (!component) {
        return false;
    }
    m_index = node->components().size();
    node->addComponent(std::move(component));

    return true;
}

void AddComponentCommand::revert(EditorSession& session)
{
    Node* node = session.findNode(m_node);
    Component* component = node && m_index ? componentAt(*node, *m_index) : nullptr;
    if (component) {
        m_detached = node->removeComponent(*component);
    }
}

RemoveComponentCommand::RemoveComponentCommand(NodeId node, std::size_t index)
    : m_node(node)
    , m_index(index)
{
}

bool RemoveComponentCommand::apply(EditorSession& session)
{
    Node* node = session.findNode(m_node);
    Component* component = node ? componentAt(*node, m_index) : nullptr;
    if (!component) {
        return false;
    }
    m_detached = node->removeComponent(*component);

    return m_detached != nullptr;
}

void RemoveComponentCommand::revert(EditorSession& session)
{
    Node* node = session.findNode(m_node);
    if (node && m_detached) {
        node->insertComponent(std::min(m_index, node->components().size()), std::move(m_detached));
    }
}

SetPropertyCommand::SetPropertyCommand(PropertyTarget target, const Property& property, PropertyValue value)
    : m_target(target)
    , m_property(property)
    , m_value(std::move(value))
{
}

bool SetPropertyCommand::apply(EditorSession& session)
{
    if (!m_previous) {
        PropertyValue previous;
        if (!getProperty(session, m_target, m_property, previous)) {
            return false;
        }
        m_previous = std::move(previous);
    }

    return setProperty(session, m_target, m_property, m_value);
}

void SetPropertyCommand::revert(EditorSession& session)
{
    if (m_previous) {
        setProperty(session, m_target, m_property, *m_previous);
    }
}

bool SetPropertyCommand::merge(const Command& next)
{
    const auto* other = dynamic_cast<const SetPropertyCommand*>(&next);
    if (!other || other->m_target != m_target || &other->m_property != &m_property) {
        return false;
    }
    m_value = other->m_value;

    return true;
}

SetTransformCommand::SetTransformCommand(NodeId node, const TransformValues& values)
    : m_node(node)
    , m_values(values)
{
}

bool SetTransformCommand::apply(EditorSession& session)
{
    auto* node = dynamic_cast<Node3D*>(session.findNode(m_node));
    if (!node) {
        return false;
    }
    if (!m_previous) {
        m_previous = node->transform().values();
    }
    node->transform().setValues(m_values);

    return true;
}

void SetTransformCommand::revert(EditorSession& session)
{
    auto* node = dynamic_cast<Node3D*>(session.findNode(m_node));
    if (node && m_previous) {
        node->transform().setValues(*m_previous);
    }
}

bool SetTransformCommand::merge(const Command& next)
{
    const auto* other = dynamic_cast<const SetTransformCommand*>(&next);
    if (!other || other->m_node != m_node) {
        return false;
    }
    m_values = other->m_values;

    return true;
}

SetParentCommand::SetParentCommand(NodeId node, NodeId parent, KeepWorld keepWorld)
    : m_node(node)
    , m_parent(parent)
    , m_keepWorld(keepWorld)
{
}

bool SetParentCommand::apply(EditorSession& session)
{
    Node* node = session.findNode(m_node);
    Node* parent = session.findNode(m_parent);
    if (!node || !parent || !canMoveUnder(*node, *parent)) {
        return false;
    }
    const auto siblings = node->parent()->children();
    m_previousParent = node->parent()->id();
    m_previousIndex = static_cast<std::size_t>(
        std::ranges::find_if(
            siblings,
            [&](const auto& child) {
                return child.get() == node;
            }
        )
        - siblings.begin()
    );
    if (const auto* node3D = dynamic_cast<const Node3D*>(node)) {
        m_previousValues = node3D->transform().values();
    }
    session.scene().setParent(*node, *parent, m_keepWorld);

    return true;
}

void SetParentCommand::revert(EditorSession& session)
{
    Node* node = session.findNode(m_node);
    if (!node) {
        return;
    }
    // Ветку вынимают и вставляют на прежнее место: так возвращается и порядок
    // среди соседей. Выбор внутри неё снимается при этом и возвращается.
    const Selection selected = session.selection();
    DetachedBranch branch = detach(session, *node);
    branch.parent = m_previousParent;
    branch.index = m_previousIndex;
    if (attach(session, branch)) {
        session.selection() = selected;
    }
    if (auto* node3D = dynamic_cast<Node3D*>(session.findNode(m_node)); node3D && m_previousValues) {
        node3D->transform().setValues(*m_previousValues);
    }
}

bool canMoveUnder(const Node& node, const Node& parent)
{
    return node.parent() && node.parent() != &parent && !isInBranch(node, parent);
}

} // namespace yqlora::editor
