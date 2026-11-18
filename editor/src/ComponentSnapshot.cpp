#include "yqlora/editor/ComponentSnapshot.hpp"

#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Component.hpp"

namespace yqlora::editor {

ComponentSnapshot::ComponentSnapshot(const TypeRegistry& registry, const Component& component)
{
    const TypeInfo* type = registry.find(component.typeName());
    if (!type) {
        return;
    }
    for (const Property* property : registry.properties(*type)) {
        m_values.emplace_back(property, property->get(component));
    }
}

std::vector<PropertyEdit> ComponentSnapshot::changes(const Component& component) const
{
    std::vector<PropertyEdit> edits;
    for (const auto& [property, value] : m_values) {
        if (PropertyValue now = property->get(component); now != value) {
            edits.push_back({property, std::move(now)});
        }
    }

    return edits;
}

void ComponentSnapshot::restore(Component& component) const
{
    for (const auto& [property, value] : m_values) {
        if (property->get(component) != value) {
            property->set(component, value);
        }
    }
}

} // namespace yqlora::editor
