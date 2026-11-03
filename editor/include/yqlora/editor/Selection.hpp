#pragma once

#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <algorithm>
#include <span>
#include <vector>

namespace yqlora::editor {

// Что выбрано в редакторе. Общее для 3D-окна и дерева узлов: выбор в одном
// месте сразу виден в другом. Можно выбрать несколько узлов; последний
// выбранный - основной: его показывает инспектор, на нём стоит манипулятор.
// Вместо узлов можно выбрать ресурс проекта в "Файлах": тогда
// инспектор показывает его. Выбор узла снимает выбор ресурса, и наоборот.
// Хранит ID, а не указатели, чтобы удалённый узел не оставил висячих ссылок.
class Selection {
public:
    // Основной выбранный узел, 0 - ничего не выбрано.
    NodeId selected() const
    {
        return m_nodes.empty() ? 0 : m_nodes.back();
    }

    std::span<const NodeId> nodes() const
    {
        return m_nodes;
    }

    bool empty() const
    {
        return m_nodes.empty();
    }

    bool isSelected(NodeId node) const
    {
        return node != 0 && std::ranges::find(m_nodes, node) != m_nodes.end();
    }

    // Выбирает только этот узел; 0 снимает выбор.
    void select(NodeId node)
    {
        m_nodes.clear();
        m_resource = 0;
        if (node != 0) {
            m_nodes.push_back(node);
        }
    }

    // Добавляет узел к выбору или убирает из него (Ctrl+клик). Добавленный становится основным.
    void toggle(NodeId node)
    {
        if (isSelected(node)) {
            deselect(node);
        } else if (node != 0) {
            m_nodes.push_back(node);
            m_resource = 0;
        }
    }

    void deselect(NodeId node)
    {
        std::erase(m_nodes, node);
    }

    // Оставляет выбранными только узлы, для которых keep(id) - true, в
    // прежнем порядке: так выделение не держит удалённых узлов.
    template <typename Keep>
    void keepNodes(Keep keep)
    {
        std::erase_if(m_nodes, [&](NodeId id) {
            return !keep(id);
        });
    }

    void clear()
    {
        m_nodes.clear();
        m_resource = 0;
    }

    // Выбранный ресурс проекта (файл); 0 - не выбран.
    ResourceId resource() const
    {
        return m_resource;
    }

    // Выбирает ресурс вместо узлов; 0 снимает выбор ресурса.
    void selectResource(ResourceId resource)
    {
        if (resource != 0) {
            m_nodes.clear();
        }
        m_resource = resource;
    }

    bool operator==(const Selection&) const = default;

private:
    std::vector<NodeId> m_nodes;
    ResourceId m_resource = 0;
};

} // namespace yqlora::editor
