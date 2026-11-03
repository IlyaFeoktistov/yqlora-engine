#include "yqlora/editor/ui/inspectors/ModelInspector.hpp"

#include "yqlora/editor/ui/core/EditorContext.hpp"
#include "yqlora/editor/ui/style/ResourceIcons.hpp"
#include "yqlora/editor/ui/widgets/DragDrop.hpp"

#include "yqlora/i18n/Translate.hpp"
#include "yqlora/render/MeshData.hpp"

#include <imgui.h>

#include <cfloat>
#include <string>

namespace yqlora::editor {

namespace {

// Масштаб импорта: меньше - модель исчезла бы.
constexpr float kMinImportScale = 0.0001f;
constexpr float kImportScaleSpeed = 0.01f;
constexpr int kMeshColumns = 4;

void drawMeshes(const ModelInspection& inspection)
{
    ImGui::Spacing();
    ImGui::Text("%s: %zu", tr("editor.resource.nodes").c_str(), inspection.nodes);
    ImGui::SeparatorText(tr("editor.resource.meshes").c_str());
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("meshes", kMeshColumns, flags)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(tr("editor.resource.vertices").c_str(), ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(tr("editor.resource.triangles").c_str(), ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(tr("editor.resource.submeshes").c_str(), ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (const MeshSummary& mesh : inspection.meshes) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const std::string label = std::string(resourceIcon(MeshData::kResourceKind)) + "  " + mesh.label;
            ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
            resourceDragSource(mesh.id, label);
            ImGui::TableNextColumn();
            ImGui::Text("%zu", mesh.vertices);
            ImGui::TableNextColumn();
            ImGui::Text("%zu", mesh.triangles);
            ImGui::TableNextColumn();
            ImGui::Text("%zu", mesh.submeshes);
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("%s", tr("editor.resource.dragHint").c_str());
}

} // namespace

void ModelInspector::reset()
{
    m_import.reset();
}

void ModelInspector::draw(EditorContext& context, ResourceId id)
{
    const auto inspection = inspectModel(context.session, id);
    if (!inspection) {
        return;
    }
    if (!m_import) {
        m_import = inspection->import;
    }
    drawMeshes(*inspection);

    ImGui::SeparatorText(tr("editor.resource.import").c_str());
    ImGui::DragFloat(
        tr("editor.resource.scale").c_str(),
        &m_import->scale,
        kImportScaleSpeed,
        kMinImportScale,
        FLT_MAX,
        "%.4g",
        ImGuiSliderFlags_AlwaysClamp
    );
    switch (importButtons(*m_import != inspection->import)) {
    case ImportAction::Apply: applyModelImport(context.session, id, *m_import); break;
    case ImportAction::Revert: m_import = inspection->import; break;
    case ImportAction::None: break;
    }
}

} // namespace yqlora::editor
