#include "yqlora/reflection/Property.hpp"

#include <algorithm>
#include <cassert>

namespace yqlora {

Property Property::forNode(std::string name, PropertyType type, NodeGetter getter, NodeSetter setter, PropertyHint hint)
{
    Property property;
    property.m_name = std::move(name);
    property.m_type = type;
    property.m_hint = hint;
    property.m_nodeGet = std::move(getter);
    property.m_nodeSet = std::move(setter);

    return property;
}

Property Property::forComponent(
    std::string name,
    PropertyType type,
    ComponentGetter getter,
    ComponentSetter setter,
    PropertyHint hint
)
{
    Property property;
    property.m_name = std::move(name);
    property.m_type = type;
    property.m_hint = hint;
    property.m_componentGet = std::move(getter);
    property.m_componentSet = std::move(setter);

    return property;
}

Property Property::forSettings(
    std::string name,
    PropertyType type,
    SettingsGetter getter,
    SettingsSetter setter,
    PropertyHint hint
)
{
    Property property;
    property.m_name = std::move(name);
    property.m_type = type;
    property.m_hint = hint;
    property.m_settingsGet = std::move(getter);
    property.m_settingsSet = std::move(setter);

    return property;
}

void Property::setEnumNames(std::vector<std::string> names)
{
    assert(m_type == PropertyType::Enum && "setEnumNames: the property is not an enum");
    m_enumNames = std::move(names);
}

PropertyValue defaultPropertyValue(PropertyType type)
{
    switch (type) {
    case PropertyType::Bool: return false;
    case PropertyType::Int: return 0;
    case PropertyType::Float: return 0.0f;
    case PropertyType::String: return std::string();
    case PropertyType::Vector2: return Vec2(0.0f);
    case PropertyType::Vector3: return Vec3(0.0f);
    case PropertyType::Vector4: return Vec4(0.0f);
    case PropertyType::Enum: return EnumValue{};
    case PropertyType::Node: return NodeRef{};
    case PropertyType::Resource: return ResourceRef{};
    case PropertyType::List: return ListValue{};
    case PropertyType::Record: return RecordValue{};
    }

    return false;
}

void Property::setElementLabels(ElementLabels labels)
{
    assert(m_type == PropertyType::List && m_componentGet && "setElementLabels: not a list of a component");
    m_elementLabels = std::move(labels);
}

void Property::setElementType(PropertyType type)
{
    assert(m_type == PropertyType::List && "setElementType: the property is not a list");
    assert(type != PropertyType::List && "setElementType: lists of lists are not supported");
    m_elementType = type;
}

void Property::setRecord(std::vector<Property> fields, RecordValue defaults)
{
    assert(
        (m_type == PropertyType::Record || (m_type == PropertyType::List && m_elementType == PropertyType::Record))
        && "setRecord: the property is not a record or a list of records"
    );
    assert(fields.size() == defaults.fields.size() && "setRecord: a default for every field");
    m_fields = std::move(fields);
    m_recordDefaults = std::move(defaults);
}

Property Property::field(std::string name, PropertyType type, PropertyHint hint)
{
    Property property;
    property.m_name = std::move(name);
    property.m_type = type;
    property.m_hint = hint;

    return property;
}

PropertyValue Property::defaultElement() const
{
    const PropertyType type = m_type == PropertyType::List ? m_elementType : m_type;

    return type == PropertyType::Record ? PropertyValue(m_recordDefaults) : defaultPropertyValue(type);
}

bool Property::accepts(const PropertyValue& value) const
{
    return acceptsAs(m_type, value);
}

bool Property::acceptsAs(PropertyType type, const PropertyValue& value) const
{
    if (static_cast<PropertyType>(value.index()) != type) {
        return false;
    }
    if (type == PropertyType::List) {
        return std::ranges::all_of(std::get<ListValue>(value).items, [this](const PropertyValue& item) {
            return acceptsAs(m_elementType, item);
        });
    }
    if (type == PropertyType::Record) {
        const auto& fields = std::get<RecordValue>(value).fields;
        if (fields.size() != m_fields.size()) {
            return false;
        }
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (!m_fields[i].accepts(fields[i])) {
                return false;
            }
        }
    }

    return true;
}

PropertyValue Property::get(const Node& node) const
{
    assert(m_nodeGet && "Property::get: this is a component property");

    return m_nodeGet(node);
}

PropertyValue Property::get(const Component& component) const
{
    assert(m_componentGet && "Property::get: this is a node property");

    return m_componentGet(component);
}

PropertyValue Property::get(const SettingsSection& section) const
{
    assert(m_settingsGet && "Property::get: this is not a settings property");

    return m_settingsGet(section);
}

bool Property::set(Node& node, const PropertyValue& value) const
{
    assert(m_nodeSet && "Property::set: this is a component property");
    if (!accepts(value)) {
        return false;
    }
    m_nodeSet(node, value);

    return true;
}

bool Property::set(Component& component, const PropertyValue& value) const
{
    assert(m_componentSet && "Property::set: this is a node property");
    if (!accepts(value)) {
        return false;
    }
    m_componentSet(component, value);

    return true;
}

bool Property::set(SettingsSection& section, const PropertyValue& value) const
{
    assert(m_settingsSet && "Property::set: this is not a settings property");
    if (!accepts(value)) {
        return false;
    }
    m_settingsSet(section, value);

    return true;
}

} // namespace yqlora
