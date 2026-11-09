#include "yqlora/reflection/ComponentLookup.hpp"

#include "yqlora/reflection/ComponentRules.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Node.hpp"

namespace yqlora {

namespace {

bool fits(const Component& component, std::string_view typeName, const TypeRegistry& registry)
{
    if (component.typeName() == typeName) {
        return true;
    }
    const TypeInfo* type = registry.find(component.typeName());

    return type && derivesFrom(registry, *type, typeName);
}

} // namespace

Component* componentOfType(const Node& node, std::string_view typeName, const TypeRegistry& registry)
{
    for (const auto& component : node.components()) {
        if (fits(*component, typeName, registry)) {
            return component.get();
        }
    }

    return nullptr;
}

std::vector<Component*> componentsOfType(const Node& node, std::string_view typeName, const TypeRegistry& registry)
{
    std::vector<Component*> found;
    for (const auto& component : node.components()) {
        if (fits(*component, typeName, registry)) {
            found.push_back(component.get());
        }
    }

    return found;
}

} // namespace yqlora
