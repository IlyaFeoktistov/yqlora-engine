#pragma once

#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/NodeId.hpp"
#include "yqlora/scene/ObjectId.hpp"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace yqlora {
class Component;
} // namespace yqlora

namespace yqlora::editor {

// Перетаскивание в редакторе. Источник и цель - последний элемент ImGui.
// Тянуть можно несколько ресурсов или узлов сразу (выделенные): цель, которой
// нужен один, берёт первый. Цель с проверкой fits не подсвечивается и не
// принимает то, что ей не подходит, а курсор над ней показывает запрет.

// Ресурсы проекта (модель, меш, текстура) из "Файлов" - в дерево сцены, окно
// вида или поле ссылки на ресурс; label - рядом с курсором.
void resourceDragSource(ResourceId id, const std::string& label);
void resourceDragSource(std::span<const ResourceId> ids, const std::string& label);

std::optional<ResourceId> acceptResourceDrop();
std::optional<std::vector<ResourceId>> acceptResourcesDrop(
    const std::function<bool(std::span<const ResourceId>)>& fits
);

// Узлы дерева сцены - к другому родителю или в поле ссылки на узел.
void nodeDragSource(NodeId id, const std::string& label);
void nodeDragSource(std::span<const NodeId> ids, const std::string& label);

std::optional<NodeId> acceptNodeDrop();
std::optional<std::vector<NodeId>> acceptNodesDrop(const std::function<bool(std::span<const NodeId>)>& fits);

// Компонент за свой заголовок в инспекторе - в поле ссылки на компонент.
struct DraggedComponent {
    NodeId node = 0;
    ComponentId component = 0;
};

void componentDragSource(const Component& component, const std::string& label);
std::optional<DraggedComponent> acceptComponentDrop(const std::function<bool(const DraggedComponent&)>& fits);

} // namespace yqlora::editor
