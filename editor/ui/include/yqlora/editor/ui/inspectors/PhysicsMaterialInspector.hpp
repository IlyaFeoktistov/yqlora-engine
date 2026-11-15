#pragma once

#include "yqlora/editor/ui/inspectors/ResourceKindInspector.hpp"

namespace yqlora::editor {

// Физический материал: трение, упругость и режимы их сведения таблицей
// свойств по полям записи PhysicsMaterialData. Правка сразу меняет материал
// в кэше (ResourceEditing), и идущая сцена чувствует её со следующего шага;
// файл пишется, когда правка закончена. Встроенный материал только читается.
class PhysicsMaterialInspector final : public ResourceKindInspector {
public:
    void reset() override;
    void draw(EditorContext& context, ResourceId id) override;
};

} // namespace yqlora::editor
