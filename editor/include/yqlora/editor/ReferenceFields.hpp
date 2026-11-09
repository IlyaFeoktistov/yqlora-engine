#pragma once

#include "yqlora/reflection/Property.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <span>
#include <string>
#include <vector>

namespace yqlora {
class Component;
class Node;
class ResourceCache;
class ResourceDatabase;
class Scene;
class TypeRegistry;
} // namespace yqlora

namespace yqlora::editor {

// Что поле ссылки (на узел, компонент, ресурс) берёт при броске и что
// предлагает в окне выбора. Тип цели - в атрибутах свойства (nodeType,
// componentType, resourceKind); у списка они относятся к его элементам.

// Узел подходит полю узла: тип nodeType или его наследник, любой - если пусто.
bool nodeFits(const Property& property, const Node& node, const TypeRegistry& registry);

// Компонент подходит полю компонента: тип componentType или наследник.
bool componentFits(const Property& property, const Component& component, const TypeRegistry& registry);

// Компоненты узла, которые подходят полю компонента, в порядке узла: пусто -
// бросок узла не принимается, один - берётся он, несколько - спросить какой.
std::vector<Component*> fittingComponents(const Property& property, const Node& node, const TypeRegistry& registry);

// Узлы сцены, подходящие полю, в порядке дерева.
std::vector<const Node*> nodeChoices(const Property& property, const Scene& scene, const TypeRegistry& registry);

// Компоненты сцены, подходящие полю, в порядке дерева и узла.
std::vector<const Component*> componentChoices(
    const Property& property,
    const Scene& scene,
    const TypeRegistry& registry
);

// Подпись компонента в поле и в окне выбора: "Lamp / MeshRenderer", у
// второго такого же на узле - "Lamp / MeshRenderer 2".
std::string componentLabel(const Component& component);

// Элементы для списка ссылок на ресурсы из брошенных файлов: каждый, что
// подходит виду поля (resourceForField), по порядку; остальные пропускаются.
std::vector<PropertyValue> resourceItems(
    const Property& property,
    std::span<const ResourceId> dropped,
    const ResourceDatabase* resources,
    ResourceCache& cache
);

// Элементы для списка ссылок на узлы из брошенных узлов, что подходят полю.
std::vector<PropertyValue> nodeItems(
    const Property& property,
    std::span<const Node* const> dropped,
    const TypeRegistry& registry
);

} // namespace yqlora::editor
