#pragma once

#include "yqlora/editor/ui/widgets/PropertyEditor.hpp"

#include <functional>

namespace yqlora {
class Component;
class Node;
class SettingsSection;
class TypeInfo;
class TypeRegistry;
} // namespace yqlora

namespace yqlora::editor {

// Новое значение свойства: его записывают командой, чтобы правку можно было отменить.
using PropertyChanged = std::function<void(const Property& property, const PropertyValue& value)>;

// Все свойства типа таблицей: подписи узкой колонкой постоянной ширины,
// остальное место - значениям. Возвращает true, если какое-то значение тянут
// мышью. overrides - у объектов экземпляров префабов (PropertyOverrides).
bool drawPropertyTable(
    const TypeRegistry& registry,
    const PropertyEditorContext& widgets,
    const TypeInfo& type,
    Node& node,
    const PropertyChanged& changed,
    const PropertyOverrides* overrides = nullptr
);
bool drawPropertyTable(
    const TypeRegistry& registry,
    const PropertyEditorContext& widgets,
    const TypeInfo& type,
    Component& component,
    const PropertyChanged& changed,
    const PropertyOverrides* overrides = nullptr
);
bool drawPropertyTable(
    const TypeRegistry& registry,
    const PropertyEditorContext& widgets,
    const TypeInfo& type,
    SettingsSection& section,
    const PropertyChanged& changed
);

} // namespace yqlora::editor
