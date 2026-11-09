#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace yqlora {

class Node;

// Узлы сцены по ID и по группам, чтобы поиск не обходил дерево. Узел
// попадает сюда, когда входит в сцену, и уходит, когда выходит; группы - в
// порядке, в каком узлы в них вошли, узел в группе один раз, пустое имя не
// группа.
class YQLORA_API NodeIndex {
public:
    void add(Node& node);
    void remove(Node& node);

    // Узел сменил ID в сцене (загрузчик выставил ID из файла).
    void changeId(Node& node, NodeId previous);

    // Узел в сцене сменил группы: previous - группы до смены.
    void changeGroups(Node& node, std::span<const std::string> previous);

    // nullptr, если такого узла в сцене нет.
    Node* find(NodeId id) const;

    std::span<Node* const> group(std::string_view name) const;

private:
    void addToGroups(Node& node, std::span<const std::string> groups);
    void removeFromGroups(Node& node, std::span<const std::string> groups);

    std::unordered_map<NodeId, Node*> m_byId;
    std::map<std::string, std::vector<Node*>, std::less<>> m_groups;
};

} // namespace yqlora
