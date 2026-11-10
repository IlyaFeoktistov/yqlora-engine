#include "yqlora/editor/ui/panels/ViewportPanel.hpp"
#include "yqlora/editor/ui/widgets/DragDrop.hpp"

#include "yqlora/editor/ui/gizmos/AxisGizmo.hpp"
#include "yqlora/editor/ui/gizmos/CameraGizmo.hpp"
#include "yqlora/editor/ui/gizmos/GridGizmo.hpp"
#include "yqlora/editor/ui/gizmos/LightGizmo.hpp"
#include "yqlora/editor/ui/gizmos/ManipulatorGizmo.hpp"
#include "yqlora/editor/ui/gizmos/SelectionGizmo.hpp"
#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"
#include "yqlora/editor/ui/widgets/PlayFrame.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/EditorSettings.hpp"
#include "yqlora/editor/Picking.hpp"
#include "yqlora/editor/SceneEditing.hpp"
#include "yqlora/editor/Snapping.hpp"
#include "yqlora/editor/ui/core/EditorHost.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/material/MaterialData.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/FrameBuilder.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/render/Renderer.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Scene.hpp"

#include <imgui.h>

#include <string>
#include <utility>

namespace yqlora::editor {

namespace {

// Наискосок сверху, как вид по умолчанию в Blender: видно все три оси сразу.
const Vec3 kStartCameraPosition(6.0f, 4.5f, 6.0f);
const Vec3 kStartCameraTarget(0.0f, 0.5f, 0.0f);
// Превью камеры: доля ширины окна сцены и отступ от его угла.
constexpr float kPreviewShare = 0.3f;
constexpr float kPreviewMargin = 12.0f;
constexpr float kPreviewBorder = 1.0f;
// Кнопки справа: тот же отступ от угла, что у панели инструментов слева.
constexpr float kToggleMargin = 8.0f;

// Кнопка-переключатель с правым краем right; right сдвигается левее неё.
// Возвращает true, если курсор над ней.
bool drawToggle(
    EditorContext& context,
    const char* icon,
    bool& enabled,
    const char* tooltipKey,
    float& right,
    float top
)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const float width = ImGui::CalcTextSize(icon).x + style.FramePadding.x * 2.0f;
    right -= width;
    ImGui::SetCursorScreenPos(ImVec2(right, top));
    right -= style.ItemSpacing.x;

    const bool wasEnabled = enabled;
    if (wasEnabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
    }
    if (ImGui::Button(icon)) {
        enabled = !enabled;
        context.settings.save();
    }
    if (wasEnabled) {
        ImGui::PopStyleColor();
    }
    ImGui::SetItemTooltip("%s", tr(tooltipKey).c_str());

    return ImGui::IsItemHovered();
}

} // namespace

ViewportPanel::ViewportPanel()
{
    // Порядок - приоритет мыши: манипулятор и оси раньше выбора кликом. Он же
    // порядок проходов вида: сетка мышь не берёт и стоит перед выбором,
    // чтобы контур выделения рисовался поверх неё.
    m_gizmos.add(std::make_unique<ManipulatorGizmo>());
    m_gizmos.add(std::make_unique<AxisGizmo>());
    m_gizmos.add(std::make_unique<CameraGizmo>());
    m_gizmos.add(std::make_unique<LightGizmo>());
    m_gizmos.add(std::make_unique<GridGizmo>());
    m_gizmos.add(std::make_unique<SelectionGizmo>());
}

void ViewportPanel::onProjectOpened(EditorContext& /*context*/)
{
    m_camera.setPosition(kStartCameraPosition);
    m_camera.lookAt(kStartCameraTarget);
    m_bringForward = true;
}

void ViewportPanel::draw(EditorContext& context)
{
    EditorSession& session = context.session;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = beginWindow(context);
    ImGui::PopStyleVar();
    // Проект открыт: впереди окно вида, а не игры, где бы ни стояли вкладки.
    if (std::exchange(m_bringForward, false)) {
        ImGui::SetWindowFocus();
    }

    const ImVec2 size = ImGui::GetContentRegionAvail();
    if (!visible || !m_view.resize(context.host.renderer(), size)) {
        ImGui::End();
        setFlying(false);

        return;
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    m_view.image(size);
    const bool hovered = ImGui::IsItemHovered();
    if (const auto dropped = acceptResourceDrop()) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const Ray ray = m_camera.screenRay({mouse.x - origin.x, mouse.y - origin.y}, {size.x, size.y});
        // Материал ложится в слот объекта под курсором, остальное ставится в сцену.
        if (session.resources().loadedKind(*dropped) == MaterialData::kResourceKind) {
            session.editing().dropMaterial(ray, {*dropped});
        } else {
            session.editing().addResource(*dropped, nullptr, dropPoint(session.scene(), ray, context.settings.snap));
        }
    }

    // Превью и его кнопка лежат поверх сцены: клики по ним до сцены не доходят.
    const bool overToggle = drawToggles(context, origin, size);
    const Camera3D* previewed = previewedCamera(context);
    const Rect preview = previewRect(origin, size);
    const bool overPreview = previewed && preview.hovered();
    const bool sceneHovered = hovered && !overToggle && !overPreview;

    m_camera.update(context.host.input(), context.deltaSeconds, sceneHovered && !m_mouseOverGizmo);
    setFlying(m_camera.flying());
    context.mouseWrapping = context.mouseWrapping || m_camera.flying();

    const float aspect = size.x / size.y;
    GizmoContext gizmoContext{
        .session = session,
        .camera = m_camera,
        .theme = context.theme,
        .settings = context.settings,
        .view = m_camera.cameraData(aspect),
        .origin = origin,
        .size = size,
        .mouseFree = sceneHovered && !m_camera.flying(),
    };
    m_mouseOverGizmo = m_gizmos.draw(gizmoContext);
    if (session.playing()) {
        drawPlayFrame(origin, size, context.theme.colors());
    }

    renderScene(context, aspect, gizmoContext);
    if (previewed) {
        drawCameraPreview(context, *previewed, preview);
    }
    ImGui::End();
}

void ViewportPanel::renderScene(EditorContext& context, float aspect, const GizmoContext& gizmoContext)
{
    const EditorHost& host = context.host;
    const Scene& scene = context.session.scene();
    FrameData frame = extractFrame(scene, aspect, previewLighting(context));
    // Окно сцены показывает кадр с экспозицией главной камеры: как в игре.
    const RenderSystem* render = scene.system<RenderSystem>();
    frame.views = {{
        .camera = m_camera.cameraData(aspect),
        .exposure = viewExposure(render ? render->mainCamera() : nullptr),
        .showShadowCascades = context.settings.viewportShadowCascades,
    }};
    // Небо сцены за объектами, а без него (выключено или в сцене нет) -
    // фон темы, первым из проходов поверх кадра, под сеткой и контуром.
    if (context.settings.viewportSky && frame.environment.sky) {
        frame.views.front().clear = CameraClear::Sky;
    } else {
        if (!m_background) {
            m_background = std::make_unique<BackgroundPass>(host.renderer().context());
        }
        m_background->setColor(context.theme.colors().viewportBackground);
        frame.views.front().passes.push_back(m_background.get());
    }
    m_gizmos.prepareView(frame.views.front(), host.renderer().context(), gizmoContext);
    m_view.render(host.renderer(), frame);
}

bool ViewportPanel::Rect::hovered() const
{
    return ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(min, ImVec2(min.x + size.x, min.y + size.y));
}

const Camera3D* ViewportPanel::previewedCamera(EditorContext& context)
{
    if (!context.settings.cameraPreview) {
        return nullptr;
    }

    return dynamic_cast<const Camera3D*>(context.session.selectedNode());
}

ViewportPanel::Rect ViewportPanel::previewRect(const ImVec2& origin, const ImVec2& size)
{
    const ImVec2 previewSize(size.x * kPreviewShare, size.y * kPreviewShare);

    return {
        .min = ImVec2(
            origin.x + size.x - previewSize.x - kPreviewMargin,
            origin.y + size.y - previewSize.y - kPreviewMargin
        ),
        .size = previewSize,
    };
}

bool ViewportPanel::drawToggles(EditorContext& context, const ImVec2& origin, const ImVec2& size)
{
    float right = origin.x + size.x - kToggleMargin;
    const float top = origin.y + kToggleMargin;
    EditorSettings& settings = context.settings;
    // Не через ||: все кнопки рисуются в каждом кадре.
    const bool overPreview = drawToggle(
        context,
        icons::kCamera3D,
        settings.cameraPreview,
        "editor.viewport.cameraPreview",
        right,
        top
    );
    const bool
        overSky = drawToggle(context, icons::kEnvironment, settings.viewportSky, "editor.viewport.sky", right, top);
    // Кнопка света предпросмотра - только у открытого префаба: у сцены свой свет.
    const bool overLighting = context.session.editingPrefab()
                              && drawToggle(
                                  context,
                                  icons::kDirectionalLight3D,
                                  settings.viewportPreviewLighting,
                                  "editor.viewport.previewLighting",
                                  right,
                                  top
                              );
    const bool overCascades = drawToggle(
        context,
        icons::kShadowCascades,
        settings.viewportShadowCascades,
        "editor.viewport.shadowCascades",
        right,
        top
    );

    return overPreview || overSky || overLighting || overCascades;
}

void ViewportPanel::drawCameraPreview(EditorContext& context, const Camera3D& camera, const Rect& rect)
{
    const EditorHost& host = context.host;
    if (!m_preview.resize(host.renderer(), rect.size)) {
        return;
    }
    const ImVec2 max(rect.min.x + rect.size.x, rect.min.y + rect.size.y);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    m_preview.image(list, rect.min, max);
    list.AddRect(rect.min, max, toImU32(context.theme.colors().cameraGizmo), 0.0f, 0, kPreviewBorder);
    list.AddText(
        ImVec2(rect.min.x + kPreviewBorder, rect.min.y - ImGui::GetTextLineHeightWithSpacing()),
        ImGui::GetColorU32(ImGuiCol_Text),
        camera.name().c_str()
    );

    // Только эта камера, как её увидит игра: без гизмо и без остальных камер.
    const float aspect = rect.size.x / rect.size.y;
    FrameData frame = extractFrame(context.session.scene(), aspect, previewLighting(context));
    frame.views = {cameraView(camera, aspect)};
    m_preview.render(host.renderer(), frame);
}

const Scene* ViewportPanel::previewLighting(EditorContext& context)
{
    return context.settings.viewportPreviewLighting ? context.session.prefabLighting() : nullptr;
}

void ViewportPanel::setFlying(bool flying)
{
    ImGuiIO& io = ImGui::GetIO();
    const bool wasFlying = (io.ConfigFlags & ImGuiConfigFlags_NoMouse) != 0;
    if (flying == wasFlying) {
        return;
    }

    // В полёте мышь и клавиатура принадлежат камере: интерфейс не должен
    // подсвечивать кнопки, через которые пролетает курсор, и печатать WASD в поля ввода.
    const ImGuiConfigFlags captured = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange
                                      | ImGuiConfigFlags_NoKeyboard;
    if (flying) {
        io.ConfigFlags |= captured;
    } else {
        io.ConfigFlags &= ~captured;
    }
}

} // namespace yqlora::editor
