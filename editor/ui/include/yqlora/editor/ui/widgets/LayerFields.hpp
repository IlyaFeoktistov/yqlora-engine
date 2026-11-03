#pragma once

#include "yqlora/editor/ui/widgets/PropertyEditor.hpp"

namespace yqlora::editor {

// Один слой узла из списка (PropertyHint::Layer). true, если выбрали другой.
bool layerCombo(int& layer, const PropertyEditorContext& context);

// Набор слоёв, флажок на каждый (PropertyHint::Layers). Маска хранится в int
// с теми же битами, см. PropertyStorage<std::uint32_t>.
bool layerMaskCombo(int& stored, const PropertyEditorContext& context);

// Имена слоёв по номеру (PropertyHint::LayerNames): число строк постоянное,
// слой 0 не переименовывается.
bool layerNamesField(ListValue& list);

} // namespace yqlora::editor
