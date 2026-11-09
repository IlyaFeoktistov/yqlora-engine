#pragma once

#include "yqlora/editor/ui/inspectors/ResourceKindInspector.hpp"
#include "yqlora/editor/ui/widgets/NameDialog.hpp"

#include "yqlora/editor/MaterialPreview.hpp"

#include <memory>
#include <optional>

namespace yqlora {
class RenderTarget;
struct MaterialData;
struct ShaderData;
} // namespace yqlora

namespace yqlora::editor {

// Материал: превью, шейдер (только шейдеры), смешивание и отсечение, поля
// блока Material с виджетами их пометок и текстуры (только текстуры, с
// превью). Правка сразу меняет материал в кэше (MaterialEditing), файл
// пишется, когда правка закончена. Материал модели только читается: его
// значения показаны, а "Сделать копию" пишет .ymat рядом с моделью.
class MaterialInspector final : public ResourceKindInspector {
public:
    MaterialInspector();
    ~MaterialInspector() override;

    void reset() override;
    void draw(EditorContext& context, ResourceId id) override;

private:
    void drawPreview(EditorContext& context, ResourceId id);
    void drawState(EditorContext& context, ResourceId id, const MaterialData& material);
    void drawFields(EditorContext& context, ResourceId id, const MaterialData& material, const ShaderData& shader);
    void drawTextures(EditorContext& context, ResourceId id, const MaterialData& material, const ShaderData& shader);
    // Кнопка и окно копии материала модели.
    void drawCopy(EditorContext& context, ResourceId id, const MaterialData& material);

    // Превью создаётся, когда инспектор впервые показан: ему нужны службы
    // сессии и рендерер.
    std::optional<MaterialPreview> m_preview;
    std::unique_ptr<RenderTarget> m_target;
    NameDialog m_copyDialog;
};

} // namespace yqlora::editor
