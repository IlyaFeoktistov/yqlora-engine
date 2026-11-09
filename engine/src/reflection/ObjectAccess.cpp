#include "yqlora/reflection/ObjectAccess.hpp"

#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Node.hpp"

#include <algorithm>

namespace yqlora {

namespace {

template <typename Object>
const Property* findProperty(const Object& object, std::string_view name, const TypeRegistry& registry)
{
    const TypeInfo* type = registry.find(object.typeName());
    if (!type) {
        return nullptr;
    }
    const auto properties = registry.properties(*type);
    const auto it = std::ranges::find(properties, name, &Property::name);

    return it != properties.end() ? *it : nullptr;
}

template <typename Object>
const Method* findMethod(const Object& object, std::string_view name, const TypeRegistry& registry)
{
    const TypeInfo* type = registry.find(object.typeName());
    if (!type) {
        return nullptr;
    }
    const auto methods = registry.methods(*type);
    const auto it = std::ranges::find(methods, name, &Method::name);

    return it != methods.end() ? *it : nullptr;
}

template <typename Object>
std::optional<PropertyValue> get(const Object& object, std::string_view name, const TypeRegistry& registry)
{
    const Property* property = findProperty(object, name, registry);

    return property ? std::optional(property->get(object)) : std::nullopt;
}

template <typename Object>
bool set(Object& object, std::string_view name, const PropertyValue& value, const TypeRegistry& registry)
{
    const Property* property = findProperty(object, name, registry);

    return property && property->set(object, value);
}

template <typename Object>
MethodCallResult call(
    Object& object,
    std::string_view name,
    std::span<const PropertyValue> arguments,
    const TypeRegistry& registry
)
{
    const Method* method = findMethod(object, name, registry);

    return method ? method->call(object, arguments) : MethodCallResult{};
}

} // namespace

std::optional<PropertyValue> getProperty(const Node& node, std::string_view name, const TypeRegistry& registry)
{
    return get(node, name, registry);
}

std::optional<PropertyValue> getProperty(
    const Component& component,
    std::string_view name,
    const TypeRegistry& registry
)
{
    return get(component, name, registry);
}

bool setProperty(Node& node, std::string_view name, const PropertyValue& value, const TypeRegistry& registry)
{
    return set(node, name, value, registry);
}

bool setProperty(Component& component, std::string_view name, const PropertyValue& value, const TypeRegistry& registry)
{
    return set(component, name, value, registry);
}

MethodCallResult callMethod(
    Node& node,
    std::string_view name,
    std::span<const PropertyValue> arguments,
    const TypeRegistry& registry
)
{
    return call(node, name, arguments, registry);
}

MethodCallResult callMethod(
    Component& component,
    std::string_view name,
    std::span<const PropertyValue> arguments,
    const TypeRegistry& registry
)
{
    return call(component, name, arguments, registry);
}

} // namespace yqlora
