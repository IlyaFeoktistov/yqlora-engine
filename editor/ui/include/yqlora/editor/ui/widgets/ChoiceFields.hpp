#pragma once

#include "yqlora/reflection/Property.hpp"

#include <string>

namespace yqlora {
class Scene;
class TypeRegistry;
} // namespace yqlora

namespace yqlora::editor {

// Значение перечисления по именам из свойства (Property::enumNames).
bool enumCombo(const Property& property, EnumValue& value);

// Имя типа компонента (PropertyHint::ComponentType): только типы, которые
// переопределяют фазы кадра. Выбранный виден всегда, даже если его модуль не
// загружен.
bool componentTypeCombo(std::string& value, const TypeRegistry& registry);

} // namespace yqlora::editor
