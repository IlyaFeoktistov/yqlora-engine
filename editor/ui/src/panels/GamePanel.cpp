#include "yqlora/editor/ui/panels/GamePanel.hpp"

#include "yqlora/editor/ui/style/Theme.hpp"
#include "yqlora/editor/ui/widgets/PlayFrame.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ui/core/EditorHost.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/render/FrameBuilder.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/scene/Scene.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <string>
#include <utility>

namespace yqlora::editor {

namespace {

constexpr const char* kViewportWindow = "###Viewport";

// Окно на экране: не свёрнуто и, если это вкладка, выбрано в своей группе.
bool isShown(const ImGuiWindow& window)
{
    if (window.Hidden || window.Collapsed) {
        return false;
    }

    return !window.DockNode || window.DockNode->VisibleWindow == &window;
}

} // namespace

void GamePanel::draw(EditorContext& context)
{
    EditorSession& session = context.session;

    // В раскладке, сохранённой до появления окна игры, его нет: первый раз
    // встаёт вкладкой рядом с окном вида, а не висит посреди экрана.
    const ImGuiWindow* viewport = maximized(context) ? nullptr : ImGui::FindWindowByName(kViewportWindow);
    if (viewport) {
        ImGui::SetNextWindowDockID(viewport->DockId, ImGuiCond_FirstUseEver);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = beginWindow(context);
    ImGui::PopStyleVar();

    // Игра началась: окно игры выходит вперёд и сразу получает ввод. Если
    // перед этим было видно окно вида, после Stop оно возвращается.
    const bool playing = session.playing();
    ImGuiWindow* sceneView = ImGui::FindWindowByName(kViewportWindow);
    if (playing && !m_wasPlaying) {
        m_returnToScene = sceneView && isShown(*sceneView);
        ImGui::SetWindowFocus();
    } else if (!playing && m_wasPlaying && std::exchange(m_returnToScene, false) && sceneView) {
        ImGui::FocusWindow(sceneView);
    }
    m_wasPlaying = playing;

    const ImVec2 size = ImGui::GetContentRegionAvail();
    if (!visible || !m_view.resize(context.host.renderer(), size)) {
        ImGui::End();

        return;
    }
    session.setGameInputFocused(playing && ImGui::IsWindowFocused());

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    m_view.image(size);

    const Scene& scene = session.scene();
    const RenderSystem* render = scene.system<RenderSystem>();
    if (render && render->mainCamera()) {
        m_view.render(context.host.renderer(), extractFrame(scene, size.x / size.y));
    } else {
        // Без камеры игре не через что показываться.
        const std::string text = tr("editor.play.noCamera");
        const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        ImGui::GetWindowDrawList()->AddRectFilled(
            origin,
            ImVec2(origin.x + size.x, origin.y + size.y),
            toImU32(context.theme.colors().viewportBackground)
        );
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(origin.x + (size.x - textSize.x) * 0.5f, origin.y + (size.y - textSize.y) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_TextDisabled),
            text.c_str()
        );
    }
    if (playing) {
        drawPlayFrame(origin, size, context.theme.colors());
    }
    ImGui::End();
}

} // namespace yqlora::editor
