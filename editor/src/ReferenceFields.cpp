#include "yqlora/editor/ReferenceFields.hpp"

#include "yqlora/editor/ResourceChoices.hpp"
#include "yqlora/reflection/ComponentRules.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/Scene.hpp"

#include <string_view>

namespace yqlora::editor {

namespace {

// Тип с именем typeName - это base или его наследник; пустой base - любой.
bool typeFits(std::string_view typeName, std::string_view base, const TypeRegistry& registry)
{
    if (base.empty() || typeName == base) {
        return true;
    }
    const TypeInfo* type = registry.find(typeName);

    return type && derivesFrom(registry, *type, base);
}

void collectNodes(const Node& node, std::vector<const Node*>& out)
{
    out.push_back(&node);
    for (const auto& child : node.children()) {
        collectNodes(*child, out);
    }
}

std::vector<const Node*> allNodes(const Scene& scene)
{
    std::vector<const Node*> nodes;
    if (const Node* root = scene.root()) {
        collectNodes(*root, nodes);
    }

    return nodes;
}

} // namespace

bool nodeFits(const Property& property, const Node& node, const TypeRegistry& registry)
{
    return typeFits(node.typeName(), property.attributes().nodeType, registry);
}

bool componentFits(const Property& property, const Component& component, const TypeRegistry& registry)
{
    return typeFits(component.typeName(), property.attributes().componentType, registry);
}

std::vector<Component*> fittingComponents(const Property& property, const Node& node, const TypeRegistry& registry)
{
    std::vector<Component*> fitting;
    for (const auto& component : node.components()) {
        if (componentFits(property, *component, registry)) {
            fitting.push_back(component.get());
        }
    }

    return fitting;
}

std::vector<const Node*> nodeChoices(const Property& property, const Scene& scene, const TypeRegistry& registry)
{
    std::vector<const Node*> choices;
    for (const Node* node : allNodes(scene)) {
        if (nodeFits(property, *node, registry)) {
            choices.push_back(node);
        }
    }

    return choices;
}

std::vector<const Component*> componentChoices(
    const Property& property,
    const Scene& scene,
    const TypeRegistry& registry
)
{
    std::vector<const Component*> choices;
    for (const Node* node : allNodes(scene)) {
        for (const Component* component : fittingComponents(property, *node, registry)) {
            choices.push_back(component);
        }
    }

    return choices;
}

std::string componentLabel(const Component& component)
{
    const Node* node = component.owner();
    std::string label = (node ? node->name() + " / " : std::string()) + std::string(component.typeName());
    if (!node) {
        return label;
    }
    // Номер нужен, только если на узле несколько компонентов этого типа.
    int number = 0;
    int same = 0;
    for (const auto& other : node->components()) {
        if (other->typeName() == component.typeName()) {
            ++same;
            if (other.get() == &component) {
                number = same;
            }
        }
    }

    return same > 1 && number > 1 ? label + " " + std::to_string(number) : label;
}

std::vector<PropertyValue> resourceItems(
    const Property& property,
    std::span<const ResourceId> dropped,
    const ResourceDatabase* resources,
    ResourceCache& cache
)
{
    const PropertyAttributes& attributes = property.attributes();
    std::vector<PropertyValue> items;
    for (const ResourceId id : dropped) {
        if (const auto
                taken = resourceForField(attributes.resourceKind, attributes.resourceUse, id, resources, cache)) {
            items.emplace_back(ResourceRef{*taken});
        }
    }

    return items;
}

std::vector<PropertyValue> nodeItems(
    const Property& property,
    std::span<const Node* const> dropped,
    const TypeRegistry& registry
)
{
    std::vector<PropertyValue> items;
    for (const Node* node : dropped) {
        if (node && nodeFits(property, *node, registry)) {
            items.emplace_back(NodeRef{node->id()});
        }
    }

    return items;
}

} // namespace yqlora::editor
