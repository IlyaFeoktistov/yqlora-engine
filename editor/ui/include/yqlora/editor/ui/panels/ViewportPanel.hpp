#pragma once

#include "yqlora/editor/ui/core/EditorPanel.hpp"
#include "yqlora/editor/ui/gizmos/Gizmo.hpp"
#include "yqlora/editor/ui/widgets/RenderView.hpp"

#include "yqlora/editor/EditorCamera.hpp"
#include "yqlora/editor/render/BackgroundPass.hpp"

#include <imgui.h>

#include <memory>

namespace yqlora {

class Camera3D;

} // namespace yqlora

namespace yqlora::editor {

// Окно сцены: кадр через свободную камеру редактора. Всё остальное поверх
// сцены (сетка, выделение, манипуляторы) делают гизмо. Во время Play окно
// показывает копию Play и правит её, а игру через её камеру показывает окно
// игры (GamePanel).
class ViewportPanel final : public EditorPanel {
public:
    ViewportPanel();

    const char* id() const override
    {
        return "Viewport";
    }

    const char* titleKey() const override
    {
        return "editor.window.viewport";
    }

    bool closable() const override
    {
        return false;
    }

    void onRendererLost() override
    {
        m_view.release();
        m_preview.release();
        m_background.reset();
        m_gizmos.onRendererLost();
    }

    void draw(EditorContext& context) override;
    void onProjectOpened(EditorContext& context) override;

private:
    // Прямоугольник на экране.
    struct Rect {
        ImVec2 min;
        ImVec2 size;

        bool hovered() const;
    };

    // Рисует кадр через камеру редактора, с гизмо.
    void renderScene(EditorContext& context, float aspect, const GizmoContext& gizmoContext);
    void setFlying(bool flying);

    // Камера для превью: выбранная, если превью включено.
    static const Camera3D* previewedCamera(EditorContext& context);
    // Угол окна сцены, где стоит превью: справа снизу, с соотношением сторон окна.
    static Rect previewRect(const ImVec2& origin, const ImVec2& size);
    // Кнопка превью справа сверху. Возвращает true, если курсор над ней.
    static bool drawPreviewToggle(EditorContext& context, const ImVec2& origin, const ImVec2& size);
    // Картинка через камеру в своей текстуре, с рамкой и именем камеры.
    void drawCameraPreview(EditorContext& context, const Camera3D& camera, const Rect& rect);

    EditorCamera m_camera;
    GizmoLayer m_gizmos;
    // Курсор был над гизмо в прошлом кадре: правая кнопка над ним не запускает полёт.
    bool m_mouseOverGizmo = false;
    // Только что открыт проект: окно вида выходит вперёд.
    bool m_bringForward = true;
    RenderView m_view;
    RenderView m_preview;
    // Создаётся на рендерере окна при первом кадре, уходит вместе с ним.
    std::unique_ptr<BackgroundPass> m_background;
};

} // namespace yqlora::editor
