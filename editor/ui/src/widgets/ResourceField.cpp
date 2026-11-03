#include "yqlora/editor/ui/widgets/ResourceField.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/ResourceIcons.hpp"
#include "yqlora/editor/ui/widgets/DragDrop.hpp"

#include "yqlora/core/Text.hpp"
#include "yqlora/editor/ResourceChoices.hpp"
#include "yqlora/editor/ResourceLabel.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/render/Renderer.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/resource/ResourceCache.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace yqlora::editor {

namespace {

constexpr const char* kPickerPopup = "##resourcePicker";
// Окно выбора: ширина в строках поля, высота списка - тоже.
constexpr float kPickerWidthLines = 14.0f;
constexpr float kPickerListLines = 12.0f;
// Превью выбранной текстуры под полем и картинки в окне выбора, в строках поля.
constexpr float kPreviewLines = 3.0f;
constexpr float kThumbnailLines = 1.5f;

void textureImage(ResourceId id, const PropertyEditorContext& context, float lines)
{
    const LoadedData<TextureData> texture = context.cache.get<TextureData>(id);
    if (!texture) {
        return;
    }
    const UiTexture ui = context.renderer.uiTexture(id, texture.revision, *texture.data);
    const float height = ImGui::GetFrameHeight() * lines;
    const float width = height * static_cast<float>(texture.data->width) / static_cast<float>(texture.data->height);
    ImGui::Image(
        static_cast<ImTextureID>(ui.id),
        ImVec2(width, height),
        ImVec2(ui.uv0.x, ui.uv0.y),
        ImVec2(ui.uv1.x, ui.uv1.y)
    );
}

// Окно выбора; true, если выбрали.
bool drawPicker(std::string_view kind, ResourceRef& value, const PropertyEditorContext& context, std::string& search)
{
    if (ImGui::IsWindowAppearing()) {
        search.clear();
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", tr("editor.resourceField.search").c_str(), &search);

    bool chosen = false;
    const auto choose = [&](ResourceId id) {
        value.id = id;
        chosen = true;
        ImGui::CloseCurrentPopup();
    };
    const float listHeight = ImGui::GetFrameHeight() * kPickerListLines;
    if (ImGui::BeginChild("##choices", ImVec2(0.0f, listHeight))) {
        if (search.empty() && ImGui::Selectable(tr("editor.inspector.none").c_str(), value.id == 0)) {
            choose(0);
        }
        bool builtinHeader = false;
        bool assetsHeader = false;
        std::string group;
        for (const ResourceChoice& choice : resourceChoices(kind, context.resources, context.cache)) {
            if (!containsIgnoringCase(choice.label + " " + choice.group, search)) {
                continue;
            }
            if (choice.builtin && !builtinHeader) {
                ImGui::SeparatorText(tr("editor.resourceField.builtin").c_str());
                builtinHeader = true;
            } else if (!choice.builtin && !assetsHeader) {
                ImGui::SeparatorText(tr("editor.resourceField.assets").c_str());
                assetsHeader = true;
            }
            if (!choice.group.empty() && choice.group != group) {
                group = choice.group;
                ImGui::TextDisabled("%s  %s", icons::kModelFile, group.c_str());
            }
            // ID строки - ID ресурса: список строится заново каждый кадр, а
            // щелчок срабатывает по отпусканию, уже в следующем кадре.
            ImGui::PushID(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(choice.id)));
            if (kind == TextureData::kResourceKind) {
                textureImage(choice.id, context, kThumbnailLines);
                ImGui::SameLine();
            }
            const std::string label = std::string(resourceIcon(kind)) + "  " + choice.label;
            if (ImGui::Selectable(label.c_str(), choice.id == value.id)) {
                choose(choice.id);
            }
            ImGui::PopID();
        }
        if (!builtinHeader && !assetsHeader && !search.empty()) {
            ImGui::TextDisabled("%s", tr("editor.resourceField.nothing").c_str());
        }
    }
    ImGui::EndChild();

    return chosen;
}

} // namespace

bool resourceField(std::string_view kind, ResourceRef& value, const PropertyEditorContext& context)
{
    static std::string search;
    const std::string kindName = tr("editor.resourceKind." + std::string(kind));
    const std::string name = value.id == 0 ? tr("editor.inspector.none") + " (" + kindName + ")"
                                           : resourceLabel(value, context.resources);
    bool changed = false;

    ImGui::BeginGroup();
    const float pickWidth = ImGui::GetFrameHeight();
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    const float fieldWidth = std::max(ImGui::GetContentRegionAvail().x - pickWidth - spacing, pickWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    const std::string label = std::string(resourceIcon(kind)) + "  " + name + "##field";
    const bool open = ImGui::Button(label.c_str(), ImVec2(fieldWidth, 0.0f));
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    ImGui::SetItemTooltip("%s", name.c_str());
    ImGui::SameLine(0.0f, spacing);
    const bool pick = ImGui::Button(icons::kPick, ImVec2(pickWidth, 0.0f));
    ImGui::SetItemTooltip("%s", tr("editor.resourceField.pick").c_str());
    if (open || pick) {
        ImGui::OpenPopup(kPickerPopup);
    }
    if (kind == TextureData::kResourceKind && value.id != 0) {
        textureImage(value.id, context, kPreviewLines);
    }
    ImGui::EndGroup();

    // Вся группа - цель для перетаскивания из "Файлов".
    if (const auto dropped = acceptResourceDrop()) {
        if (const auto taken = resourceForField(kind, *dropped, context.resources, context.cache)) {
            value.id = *taken;
            changed = true;
        }
    }

    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFrameHeight() * kPickerWidthLines, 0.0f));
    if (ImGui::BeginPopup(kPickerPopup)) {
        ImGui::TextDisabled("%s: %s", tr("editor.resourceField.choose").c_str(), kindName.c_str());
        changed = drawPicker(kind, value, context, search) || changed;
        ImGui::EndPopup();
    }

    return changed;
}

} // namespace yqlora::editor
