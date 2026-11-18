#pragma once

#include "yqlora/editor/ComponentSnapshot.hpp"

#include <vector>

namespace yqlora {
class Node;
class ResourceCache;
class TypeInfo;
class TypeRegistry;
} // namespace yqlora

namespace yqlora::editor {

// Как редактор настраивает под узел компонент типа type, который человек
// добавляет на node: коллайдер подгоняется под меши узла. Значения, которые
// отличаются от значений нового компонента; пусто - настраивать нечего.
// Одно место для таких правил: новый тип с настройкой добавляется сюда.
std::vector<PropertyEdit> addedComponentSetup(
    const TypeRegistry& registry,
    const TypeInfo& type,
    const Node& node,
    ResourceCache& resources
);

} // namespace yqlora::editor
