#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/reflection/Record.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>

namespace yqlora {

// Значения свойств в JSON и обратно: одно правило для всех файлов движка,
// где лежат свойства (сцены, потом настройки проекта).

// Вектор glm - массивом чисел.
template <typename Vec>
nlohmann::json vectorToJson(const Vec& vec)
{
    nlohmann::json result = nlohmann::json::array();
    for (glm::length_t i = 0; i < Vec::length(); ++i) {
        result.push_back(vec[i]);
    }

    return result;
}

template <typename Vec>
std::optional<Vec> vectorFromJson(const nlohmann::json& json)
{
    if (!json.is_array() || json.size() != static_cast<std::size_t>(Vec::length())) {
        return std::nullopt;
    }
    Vec result;
    for (glm::length_t i = 0; i < Vec::length(); ++i) {
        const nlohmann::json& item = json[static_cast<std::size_t>(i)];
        if (!item.is_number()) {
            return std::nullopt;
        }
        result[i] = item.get<float>();
    }

    return result;
}

YQLORA_API nlohmann::json valueToJson(const Property& property, const PropertyValue& value);

// nullopt, если значение не того типа или такого значения перечисления нет.
YQLORA_API std::optional<PropertyValue> valueFromJson(const nlohmann::json& json, const Property& property);

// Значение свойства в объекте JSON: под его именем, а если его нет - под
// прежним именем (PropertyAttributes::formerly). nullptr, если нет ни там, ни там.
YQLORA_API const nlohmann::json* findPropertyJson(const nlohmann::json& object, const Property& property);

// Свойства объекта JSON, которых у типа нет ни под именем, ни под прежним
// именем: например, поле убрали из кода. Пустой объект, если таких нет.
YQLORA_API nlohmann::json unknownPropertiesJson(
    const TypeRegistry& registry,
    const TypeInfo& type,
    const nlohmann::json& object
);

// Свойства с атрибутом "не сохранять" не пишутся.
template <typename Object>
nlohmann::json propertiesToJson(const TypeRegistry& registry, const TypeInfo& type, const Object& object)
{
    nlohmann::json result = nlohmann::json::object();
    for (const Property* property : registry.properties(type)) {
        if (property->attributes().saved) {
            result[property->name()] = valueToJson(*property, property->get(object));
        }
    }

    return result;
}

// Отсутствующие свойства остаются значениями по умолчанию, лишние игнорируются:
// когда в коде игры добавляют или убирают поле, уже сохранённые сцены открываются.
template <typename Object>
void propertiesFromJson(const TypeRegistry& registry, const TypeInfo& type, Object& object, const nlohmann::json& json)
{
    if (!json.is_object()) {
        return;
    }
    for (const Property* property : registry.properties(type)) {
        const nlohmann::json* found = property->attributes().saved ? findPropertyJson(json, *property) : nullptr;
        if (!found) {
            continue;
        }
        const auto value = valueFromJson(*found, *property);
        if (!value) {
            log::warn("Property '{}.{}' has a wrong value, skipped", type.name(), property->name());
            continue;
        }
        property->set(object, *value);
    }
}

// Запись T объектом JSON по именам полей.
template <Record T>
nlohmann::json recordToJson(const T& value)
{
    return valueToJson(recordProperty<T>(), PropertyStorage<T>::store(value));
}

// Поля, которых в объекте нет, - по умолчанию T{}. nullopt, если поле не
// того типа или такого значения перечисления нет.
template <Record T>
std::optional<T> recordFromJson(const nlohmann::json& json)
{
    if (!json.is_object()) {
        return std::nullopt;
    }
    const auto value = valueFromJson(json, recordProperty<T>());
    if (!value) {
        return std::nullopt;
    }

    return PropertyStorage<T>::load(std::get<RecordValue>(*value));
}

} // namespace yqlora
