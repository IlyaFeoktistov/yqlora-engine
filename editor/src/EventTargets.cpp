#include "yqlora/editor/EventTargets.hpp"

#include "yqlora/reflection/Method.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Component.hpp"

#include <algorithm>

namespace yqlora::editor {

std::span<const EventParameter> eventParametersOf(
    const Component& component,
    std::string_view eventName,
    const TypeRegistry& registry
)
{
    const TypeInfo* type = registry.find(component.typeName());
    if (!type) {
        return {};
    }
    for (const EventInfo* event : registry.events(*type)) {
        if (event->name == eventName) {
            return event->parameters;
        }
    }

    return {};
}

bool methodFitsEvent(const Method& method, std::span<const EventParameter> parameters)
{
    // Подписчик - действие: метод с результатом (findChild, worldPosition) - запрос.
    if (method.result()) {
        return false;
    }
    const auto own = method.parameters();
    if (own.empty()) {
        return true;
    }

    return std::ranges::equal(own, parameters, [](const MethodParameter& a, const EventParameter& b) {
        return a.type == b.type;
    });
}

std::vector<std::string> eventMethods(
    std::string_view typeName,
    std::span<const EventParameter> parameters,
    const TypeRegistry& registry
)
{
    std::vector<std::string> names;
    const TypeInfo* type = registry.find(typeName);
    if (!type) {
        return names;
    }
    for (const Method* method : registry.methods(*type)) {
        if (methodFitsEvent(*method, parameters)) {
            names.push_back(method->name());
        }
    }

    return names;
}

} // namespace yqlora::editor
