#include "yqlora/reflection/PropertyJson.hpp"

#include "yqlora/core/JsonId.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace yqlora {

namespace {

using Json = nlohmann::json;

// Ключи ссылки на компонент: {"node": "...", "component": "..."}.
constexpr const char* kComponentRefNode = "node";
constexpr const char* kComponentRefComponent = "component";

std::optional<PropertyValue> componentRefFromJson(const Json& json)
{
    if (!json.is_object() || !json.contains(kComponentRefNode) || !json.contains(kComponentRefComponent)) {
        return std::nullopt;
    }
    const auto node = idFromJson(json[kComponentRefNode]);
    const auto component = idFromJson(json[kComponentRefComponent]);
    if (!node || !component) {
        return std::nullopt;
    }

    return ComponentRef<>{*node, *component};
}

std::optional<PropertyValue> enumFromJson(const Json& json, const Property& property)
{
    if (json.is_number_integer()) {
        return EnumValue{json.get<int>()};
    }
    if (!json.is_string()) {
        return std::nullopt;
    }
    const auto& names = property.enumNames();
    const auto it = std::ranges::find(names, json.get<std::string>());
    if (it == names.end()) {
        return std::nullopt;
    }

    return EnumValue{static_cast<int>(it - names.begin())};
}

std::optional<PropertyValue> fromJson(const Json& json, PropertyType type, const Property& property);

// Элементы списка того типа, что у свойства; один неверный элемент портит весь список.
std::optional<PropertyValue> listFromJson(const Json& json, const Property& property)
{
    if (!json.is_array()) {
        return std::nullopt;
    }
    ListValue list;
    list.items.reserve(json.size());
    for (const Json& item : json) {
        auto value = fromJson(item, property.elementType(), property);
        if (!value) {
            return std::nullopt;
        }
        list.items.push_back(std::move(*value));
    }

    return list;
}

// Объект с полями по именам (и прежним именам). Недостающее поле - значение
// по умолчанию, лишние не читаются: поле в структуре можно добавить или
// убрать, файлы читаются.
std::optional<PropertyValue> recordFromJson(const Json& json, const Property& property)
{
    if (!json.is_object()) {
        return std::nullopt;
    }
    const auto& fields = property.fields();
    RecordValue record = std::get<RecordValue>(property.defaultElement());
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const Json* found = findPropertyJson(json, fields[i]);
        if (!found) {
            continue;
        }
        auto value = valueFromJson(*found, fields[i]);
        if (!value) {
            return std::nullopt;
        }
        record.fields[i] = std::move(*value);
    }

    return record;
}

} // namespace

const Json* findPropertyJson(const Json& object, const Property& property)
{
    if (const auto it = object.find(property.name()); it != object.end()) {
        return &*it;
    }
    for (const std::string& name : property.attributes().formerly) {
        if (const auto it = object.find(name); it != object.end()) {
            return &*it;
        }
    }

    return nullptr;
}

Json unknownPropertiesJson(const TypeRegistry& registry, const TypeInfo& type, const Json& object)
{
    Json result = Json::object();
    if (!object.is_object()) {
        return result;
    }
    const std::vector<const Property*> properties = registry.properties(type);
    const auto known = [&](const std::string& key) {
        return std::ranges::any_of(properties, [&](const Property* property) {
            return property->name() == key
                   || std::ranges::find(property->attributes().formerly, key) != property->attributes().formerly.end();
        });
    };
    for (const auto& [key, value] : object.items()) {
        if (!known(key)) {
            result[key] = value;
        }
    }

    return result;
}

Json valueToJson(const Property& property, const PropertyValue& value)
{
    return std::visit(
        [&](const auto& v) -> Json {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Vec2> || std::is_same_v<T, Vec3> || std::is_same_v<T, Vec4>) {
                return vectorToJson(v);
            } else if constexpr (std::is_same_v<T, EnumValue>) {
                // Именем, чтобы порядок значений в C++ можно было менять. Без имени - числом.
                const auto& names = property.enumNames();
                const auto index = static_cast<std::size_t>(v.value);
                if (v.value >= 0 && index < names.size()) {
                    return names[index];
                }

                return v.value;
            } else if constexpr (std::is_same_v<T, NodeRef> || std::is_same_v<T, ResourceRef>) {
                return idToJson(v.id);
            } else if constexpr (std::is_same_v<T, ComponentRef<>>) {
                return Json{{kComponentRefNode, idToJson(v.node)}, {kComponentRefComponent, idToJson(v.component)}};
            } else if constexpr (std::is_same_v<T, RecordValue>) {
                Json object = Json::object();
                const auto& fields = property.fields();
                for (std::size_t i = 0; i < fields.size() && i < v.fields.size(); ++i) {
                    if (fields[i].attributes().saved) {
                        object[fields[i].name()] = valueToJson(fields[i], v.fields[i]);
                    }
                }

                return object;
            } else if constexpr (std::is_same_v<T, ListValue>) {
                Json items = Json::array();
                for (const PropertyValue& item : v.items) {
                    items.push_back(valueToJson(property, item));
                }

                return items;
            } else {
                return Json(v);
            }
        },
        value
    );
}

std::optional<PropertyValue> valueFromJson(const Json& json, const Property& property)
{
    return fromJson(json, property.type(), property);
}

namespace {

std::optional<PropertyValue> fromJson(const Json& json, PropertyType type, const Property& property)
{
    switch (type) {
    case PropertyType::Bool:
        if (json.is_boolean()) {
            return json.get<bool>();
        }
        break;
    case PropertyType::Int:
        if (json.is_number_integer()) {
            return json.get<int>();
        }
        break;
    case PropertyType::Float:
        if (json.is_number()) {
            return json.get<float>();
        }
        break;
    case PropertyType::String:
        if (json.is_string()) {
            return json.get<std::string>();
        }
        break;
    case PropertyType::Vector2: return vectorFromJson<Vec2>(json);
    case PropertyType::Vector3: return vectorFromJson<Vec3>(json);
    case PropertyType::Vector4: return vectorFromJson<Vec4>(json);
    case PropertyType::Enum: return enumFromJson(json, property);
    case PropertyType::Node:
        if (const auto id = idFromJson(json)) {
            return NodeRef{*id};
        }
        break;
    case PropertyType::Resource:
        if (const auto id = idFromJson(json)) {
            return ResourceRef{*id};
        }
        break;
    case PropertyType::Component: return componentRefFromJson(json);
    case PropertyType::List: return listFromJson(json, property);
    case PropertyType::Record: return recordFromJson(json, property);
    }

    return std::nullopt;
}

} // namespace

} // namespace yqlora
