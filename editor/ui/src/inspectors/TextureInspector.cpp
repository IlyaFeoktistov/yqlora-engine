#include "yqlora/editor/ui/inspectors/TextureInspector.hpp"

#include "yqlora/editor/ui/core/EditorContext.hpp"
#include "yqlora/editor/ui/core/EditorHost.hpp"

#include "yqlora/editor/TextureInspection.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/render/Renderer.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>

namespace yqlora::editor {

namespace {

// Превью не выше этой доли ширины панели.
constexpr float kPreviewMaxAspect = 1.0f;

template <NamedEnum E>
bool enumCombo(const char* label, E& value)
{
    bool changed = false;
    if (ImGui::BeginCombo(label, std::string(enumName(value)).c_str())) {
        for (std::size_t i = 0; i < std::size(EnumNames<E>::names); ++i) {
            const auto option = static_cast<E>(i);
            if (ImGui::Selectable(std::string(EnumNames<E>::names[i]).c_str(), option == value)) {
                value = option;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}

void drawPreview(EditorContext& context, ResourceId id, const LoadedData<TextureData>& texture)
{
    ImGui::Spacing();
    ImGui::Text("%s: %d x %d", tr("editor.resource.size").c_str(), texture.data->width, texture.data->height);
    const UiTexture ui = context.host.renderer().uiTexture(id, texture.revision, *texture.data);
    const float width = ImGui::GetContentRegionAvail().x;
    const float aspect = static_cast<float>(texture.data->height) / static_cast<float>(texture.data->width);
    const float height = width * std::min(aspect, kPreviewMaxAspect);
    const float shownWidth = height / aspect;
    ImGui::Image(
        static_cast<ImTextureID>(ui.id),
        ImVec2(shownWidth, height),
        ImVec2(ui.uv0.x, ui.uv0.y),
        ImVec2(ui.uv1.x, ui.uv1.y)
    );
}

} // namespace

void TextureInspector::reset()
{
    m_import.reset();
}

void TextureInspector::draw(EditorContext& context, ResourceId id)
{
    const auto inspection = inspectTexture(context.session, id);
    if (!inspection) {
        return;
    }
    if (!m_import) {
        m_import = inspection->import;
    }
    drawPreview(context, id, inspection->texture);

    ImGui::SeparatorText(tr("editor.resource.import").c_str());
    enumCombo(tr("editor.resource.colorSpace").c_str(), m_import->colorSpace);
    enumCombo(tr("editor.resource.filter").c_str(), m_import->filter);
    enumCombo(tr("editor.resource.wrap").c_str(), m_import->wrap);
    ImGui::Checkbox(tr("editor.resource.mipmaps").c_str(), &m_import->mipmaps);
    switch (importButtons(*m_import != inspection->import)) {
    case ImportAction::Apply: applyTextureImport(context.session, id, *m_import); break;
    case ImportAction::Revert: m_import = inspection->import; break;
    case ImportAction::None: break;
    }
}

} // namespace yqlora::editor
