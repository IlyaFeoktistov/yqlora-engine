#include "yqlora/scene/NodeIndex.hpp"

#include "yqlora/scene/Node.hpp"

#include <algorithm>

namespace yqlora {

void NodeIndex::add(Node& node)
{
    m_byId[node.id()] = &node;
    addToGroups(node, node.groups());
}

void NodeIndex::remove(Node& node)
{
    // Узел с тем же ID мог занять запись позже: убирается только своя.
    if (const auto it = m_byId.find(node.id()); it != m_byId.end() && it->second == &node) {
        m_byId.erase(it);
    }
    removeFromGroups(node, node.groups());
}

void NodeIndex::changeId(Node& node, NodeId previous)
{
    if (const auto it = m_byId.find(previous); it != m_byId.end() && it->second == &node) {
        m_byId.erase(it);
    }
    m_byId[node.id()] = &node;
}

void NodeIndex::changeGroups(Node& node, std::span<const std::string> previous)
{
    removeFromGroups(node, previous);
    addToGroups(node, node.groups());
}

Node* NodeIndex::find(NodeId id) const
{
    const auto it = m_byId.find(id);

    return it != m_byId.end() ? it->second : nullptr;
}

std::span<Node* const> NodeIndex::group(std::string_view name) const
{
    const auto it = m_groups.find(name);
    if (it == m_groups.end()) {
        return {};
    }

    return it->second;
}

void NodeIndex::addToGroups(Node& node, std::span<const std::string> groups)
{
    for (const std::string& name : groups) {
        if (name.empty()) {
            continue;
        }
        std::vector<Node*>& members = m_groups[name];
        if (std::ranges::find(members, &node) == members.end()) {
            members.push_back(&node);
        }
    }
}

void NodeIndex::removeFromGroups(Node& node, std::span<const std::string> groups)
{
    for (const std::string& name : groups) {
        const auto it = m_groups.find(name);
        if (it == m_groups.end()) {
            continue;
        }
        std::erase(it->second, &node);
        if (it->second.empty()) {
            m_groups.erase(it);
        }
    }
}

} // namespace yqlora
