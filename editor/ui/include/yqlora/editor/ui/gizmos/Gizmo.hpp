#pragma once

#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/ViewPass.hpp"

#include <imgui.h>

#include <memory>
#include <vector>

namespace yqlora::editor {

class EditorCamera;
class EditorSession;
class EditorSettings;
class Theme;

// Всё, что нужно гизмо, чтобы нарисоваться поверх окна сцены.
struct GizmoContext {
    EditorSession& session;
    EditorCamera& camera;
    const Theme& theme;
    // Привычки человека: привязка при перемещении.
    EditorSettings& settings;
    // Камера, через которую сейчас видна сцена.
    CameraData view;
    // Где картинка сцены на экране, в координатах интерфейса.
    ImVec2 origin;
    ImVec2 size;
    // Курсор над окном сцены, и его ещё не занял гизмо раньше в списке.
    bool mouseFree = false;
};

// Вспомогательная графика и инструменты редактора поверх сцены: сетка,
// контур выделения, камеры, оси, манипуляторы. В игре их нет.
class Gizmo {
public:
    virtual ~Gizmo() = default;

    // Что гизмо добавляет к виду сцены: свои проходы (сетка, контур),
    // созданные на рендерере renderer.
    virtual void prepareView(ViewData& /*view*/, const RenderContext& /*renderer*/, const GizmoContext& /*context*/)
    {
    }

    // Рендерер пересоздают: проходы на старом больше не нужны.
    virtual void onRendererLost()
    {
    }

    // Рисует поверх кадра и обрабатывает мышь. Возвращает true, если курсор
    // занят этим гизмо: тогда гизмо дальше по списку клик не получают.
    virtual bool draw(GizmoContext& /*context*/)
    {
        return false;
    }
};

// Набор гизмо окна сцены. Порядок - это приоритет мыши: первый в списке
// получает клик первым.
class GizmoLayer {
public:
    void add(std::unique_ptr<Gizmo> gizmo);

    void prepareView(ViewData& view, const RenderContext& renderer, const GizmoContext& context) const;
    void onRendererLost() const;

    // Возвращает true, если курсор занят каким-то гизмо.
    bool draw(GizmoContext& context);

private:
    std::vector<std::unique_ptr<Gizmo>> m_gizmos;
};

} // namespace yqlora::editor
