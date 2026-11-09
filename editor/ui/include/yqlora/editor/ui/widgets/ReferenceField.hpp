#pragma once

#include "yqlora/editor/ui/widgets/PropertyEditor.hpp"

#include "yqlora/reflection/Property.hpp"

namespace yqlora::editor {

// Поля ссылок на узел и на компонент; что они берут - в ReferenceFields.hpp.
// Щелчок по заполненному полю показывает цель в дереве сцены, по пустому или
// по кнопке справа - окно выбора с поиском. Узел бросают из дерева;
// компонент - узлом (если подходящих на нём несколько, спрашивается какой)
// или за заголовок в инспекторе. Неподходящее поле не принимает.
// true, если значение поменяли.
bool nodeField(const Property& property, NodeRef& value, const PropertyEditorContext& context);
bool componentField(const Property& property, ComponentRef<>& value, const PropertyEditorContext& context);

} // namespace yqlora::editor
