#include "yqlora/editor/ui/inspectors/ShaderInspector.hpp"

#include "yqlora/editor/ui/core/EditorContext.hpp"
#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/CompilerDiagnostics.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/platform/Platform.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <imgui.h>

#include <sstream>
#include <string>

namespace yqlora::editor {

namespace {

constexpr int kDeclarationColumns = 2;

const char* typeName(rhi::ValueType type)
{
    switch (type) {
    case rhi::ValueType::Float: return "float";
    case rhi::ValueType::Vec2: return "vec2";
    case rhi::ValueType::Vec3: return "vec3";
    case rhi::ValueType::Vec4: return "vec4";
    case rhi::ValueType::Int: return "int";
    case rhi::ValueType::IVec2: return "ivec2";
    case rhi::ValueType::IVec3: return "ivec3";
    case rhi::ValueType::IVec4: return "ivec4";
    case rhi::ValueType::UInt: return "uint";
    case rhi::ValueType::Mat3: return "mat3";
    case rhi::ValueType::Mat4: return "mat4";
    case rhi::ValueType::Other: break;
    }

    return "?";
}

// Строки ошибки компиляции ("файл:строка: текст"); щелчок открывает файл.
void drawErrors(EditorContext& context, const std::string& message)
{
    const auto& project = context.session.project();
    std::istringstream lines(message);
    std::string line;
    ImGui::PushStyleColor(ImGuiCol_Text, toImVec4(context.theme.colors().errorText));
    while (std::getline(lines, line)) {
        const CompilerDiagnostic diagnostic = parseCompilerOutput(line);
        ImGui::PushID(line.c_str());
        if (ImGui::Selectable(line.c_str()) && project && !diagnostic.file.empty()) {
            platform::openWithSystem(project->directory() / fromUtf8(diagnostic.file));
        }
        ImGui::PopID();
    }
    ImGui::PopStyleColor();
}

} // namespace

void ShaderInspector::draw(EditorContext& context, ResourceId id)
{
    const auto& project = context.session.project();
    const auto path = project ? project->resources().path(id) : std::nullopt;
    if (path && ImGui::Button((std::string(icons::kCodeFile) + "  " + tr("editor.shader.open")).c_str())) {
        platform::openWithSystem(project->directory() / *path);
    }
    ResourceCache& cache = context.session.resources();
    const ShaderData* shader = cache.find<ShaderData>(id);
    if (!shader) {
        const ResourceStatus* status = cache.status(id);
        if (status && !status->message.empty()) {
            ImGui::SeparatorText(tr("editor.shader.errors").c_str());
            drawErrors(context, status->message);
        }

        return;
    }
    ImGui::TextDisabled("%s", tr("editor.shader.compiled").c_str());
    if (!shader->fields.empty() && ImGui::BeginTable("fields", kDeclarationColumns, ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(tr("editor.shader.field").c_str());
        ImGui::TableSetupColumn(tr("editor.shader.type").c_str());
        ImGui::TableHeadersRow();
        for (const MaterialField& field : shader->fields) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(field.name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(field.color ? "color" : typeName(field.type));
        }
        ImGui::EndTable();
    }
    if (!shader->textures.empty()) {
        ImGui::SeparatorText(tr("editor.material.textures").c_str());
        for (const MaterialTexture& texture : shader->textures) {
            ImGui::BulletText("%s  (%s)", texture.name.c_str(), std::string(enumName(texture.defaultTexture)).c_str());
        }
    }
}

} // namespace yqlora::editor
