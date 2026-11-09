#pragma once

#include "yqlora/editor/ui/widgets/PropertyEditor.hpp"

#include "yqlora/reflection/EventInfo.hpp"

#include <span>

namespace yqlora::editor {

// Постоянные подписчики события (PropertyHint::EventTargets): у каждого -
// узел (бросают из дерева), сам узел или его компонент и метод из тех, что
// слушают событие с parameters; удаление строки. Внизу "Добавить", на неё
// тоже можно бросить узел. true, если список поменяли.
bool eventTargetsField(
    const Property& property,
    ListValue& list,
    std::span<const EventParameter> parameters,
    const PropertyEditorContext& context
);

} // namespace yqlora::editor
