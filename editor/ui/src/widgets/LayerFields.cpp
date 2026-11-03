#include "yqlora/editor/ui/widgets/LayerFields.hpp"

#include "yqlora/editor/ui/style/Theme.hpp"
#include "yqlora/editor/ui/widgets/Checkbox.hpp"

#include "yqlora/i18n/Translate.hpp"
#include "yqlora/scene/Layers.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <bit>
#include <string>

namespace yqlora::editor {

namespace {

// Имя слоя, а у слоя без имени - номер.

std::string layerLabel(int layer, std::span<const std::string> names)
{
    const auto index = static_cast<std::size_t>(layer);

    return index < names.size() && !names[index].empty() ? names[index] : std::to_string(layer);
}

// Слой без имени не используется и в списках не показывается, если он не выбран

// сейчас. Без проекта имён нет вовсе, тогда видны все.

bool layerListed(int layer, std::span<const std::string> names, bool selected)
{
    const auto index = static_cast<std::size_t>(layer);

    return selected || names.empty() || (index < names.size() && !names[index].empty());
}

// Последний пункт списков слоёв. Вызывается внутри открытого списка.

void editLayersItem(const PropertyEditorContext& context, bool& requested)
{
    if (!context.editLayers) {
        return;
    }
    ImGui::Separator();
    if (ImGui::Selectable(tr("editor.layers.edit").c_str())) {
        requested = true;
    }
}

std::string maskPreview(LayerMask mask, std::span<const std::string> names)
{
    if (mask == kAllLayers) {
        return tr("editor.layers.everything");
    }
    if (mask == 0) {
        return tr("editor.layers.nothing");
    }
    std::string result;
    for (int i = 0; i < kLayerCount; ++i) {
        if (inMask(mask, i)) {
            result += (result.empty() ? "" : ", ") + layerLabel(i, names);
        }
    }

    return result;
}

// Маска хранится в int с теми же битами, см. PropertyStorage<std::uint32_t>.

// Имена слоёв по номеру: число строк постоянное, слой 0 не переименовывается.

} // namespace

bool layerCombo(int& layer, const PropertyEditorContext& context)
{
    bool changed = false;
    bool editRequested = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##value", layerLabel(layer, context.layerNames).c_str())) {
        for (int i = 0; i < kLayerCount; ++i) {
            if (!layerListed(i, context.layerNames, i == layer)) {
                continue;
            }
            if (ImGui::Selectable(layerLabel(i, context.layerNames).c_str(), i == layer)) {
                layer = i;
                changed = true;
            }
        }
        editLayersItem(context, editRequested);
        ImGui::EndCombo();
    }
    if (editRequested) {
        context.editLayers();
    }

    return changed;
}

bool layerMaskCombo(int& stored, const PropertyEditorContext& context)
{
    auto mask = std::bit_cast<LayerMask>(stored);
    bool changed = false;
    bool editRequested = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##value", maskPreview(mask, context.layerNames).c_str())) {
        if (ImGui::Selectable(tr("editor.layers.everything").c_str(), mask == kAllLayers)) {
            mask = kAllLayers;
            changed = true;
        }
        if (ImGui::Selectable(tr("editor.layers.nothing").c_str(), mask == 0)) {
            mask = 0;
            changed = true;
        }
        ImGui::Separator();
        for (int i = 0; i < kLayerCount; ++i) {
            bool on = inMask(mask, i);
            if (!layerListed(i, context.layerNames, on)) {
                continue;
            }
            ImGui::PushID(i);
            if (checkbox(layerLabel(i, context.layerNames).c_str(), &on, context.metrics.checkboxPadding)) {
                mask ^= LayerMask{1} << i;
                changed = true;
            }
            ImGui::PopID();
        }
        editLayersItem(context, editRequested);
        ImGui::EndCombo();
    }
    stored = std::bit_cast<int>(mask);
    if (editRequested) {
        context.editLayers();
    }

    return changed;
}

bool layerNamesField(ListValue& list)
{
    bool changed = false;
    for (std::size_t i = 0; i < list.items.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%2zu", i);
        ImGui::SameLine();
        ImGui::BeginDisabled(i == 0);
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed = ImGui::InputText("##name", &std::get<std::string>(list.items[i])) || changed;
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    return changed;
}

} // namespace yqlora::editor
