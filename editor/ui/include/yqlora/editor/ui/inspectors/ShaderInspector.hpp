#pragma once

#include "yqlora/editor/ui/inspectors/ResourceKindInspector.hpp"

namespace yqlora::editor {

// Шейдер: ошибки последней компиляции строками (щелчок открывает файл),
// объявленные поля блока Material и текстуры, "Открыть" - файл в программе
// системы. Сам шейдер правится вне редактора.
class ShaderInspector final : public ResourceKindInspector {
public:
    void reset() override
    {
    }

    void draw(EditorContext& context, ResourceId id) override;
};

} // namespace yqlora::editor
