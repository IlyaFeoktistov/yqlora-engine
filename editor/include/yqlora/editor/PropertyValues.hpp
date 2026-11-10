#pragma once

#include "yqlora/reflection/Property.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace yqlora {
class Component;
class Node;
class SettingsSection;
class TypeInfo;
class TypeRegistry;
} // namespace yqlora

namespace yqlora::editor {

// Значение свойства у только что созданного объекта того же типа: к нему
// возвращает пункт "Сбросить". Встроенный компонент (Transform) создаёт сам
// узел, поэтому для него берётся новый узел того же типа. nullopt, если
// такой объект не создать.
std::optional<PropertyValue> defaultValue(const TypeRegistry& registry, const Node& node, const Property& property);
std::optional<PropertyValue> defaultValue(
    const TypeRegistry& registry,
    const Component& component,
    const Property& property
);
std::optional<PropertyValue> defaultValue(
    const TypeRegistry& registry,
    const SettingsSection& section,
    const Property& property
);

// Свойство видно в инспекторе у этого объекта: shown и условие shownWhen
// по значению другого его свойства. Условие с неизвестным свойством или не
// перечислением не скрывает.
bool propertyShown(const TypeRegistry& registry, const TypeInfo& type, const Node& node, const Property& property);
bool propertyShown(
    const TypeRegistry& registry,
    const TypeInfo& type,
    const Component& component,
    const Property& property
);
bool propertyShown(
    const TypeRegistry& registry,
    const TypeInfo& type,
    const SettingsSection& section,
    const Property& property
);

// Значение текстом для буфера обмена: тот же JSON, что в файлах, поэтому
// его можно вставить и в другое свойство того же вида, и из другой программы.
std::string valueToText(const Property& property, const PropertyValue& value);

// nullopt, если текст не значение этого свойства. В строковое свойство
// годится любой текст.
std::optional<PropertyValue> valueFromText(const Property& property, std::string_view text);

// Одно число вектора (X, Y, Z, W) текстом и обратно: меню одного поля.
std::string numberToText(float value);
std::optional<float> numberFromText(std::string_view text);

// Числа вектора внутри значения: X, Y, Z, W. Пусто, если значение не вектор.
std::span<float> vectorComponents(PropertyValue& value);

} // namespace yqlora::editor
