#include "yqlora/editor/ui/widgets/InputControlField.hpp"

#include "yqlora/i18n/Translate.hpp"
#include "yqlora/input/Input.hpp"
#include "yqlora/input/InputControl.hpp"

#include <imgui.h>

#include <optional>
#include <string>

namespace yqlora::editor {

namespace {

constexpr const char* kWaitingForControl = "##waitingForControl";

// Первый орган, заданный в этом кадре нажатием (см. wasControlActivated);

// Escape не считается: им отменяют ожидание.

std::optional<InputControl> activatedControl(const Input& input)
{
    for (const InputControl& control : allInputControls()) {
        if (control != InputControl{Key::Escape} && wasControlActivated(input, control)) {
            return control;
        }
    }

    return std::nullopt;
}

// Привязка действия: кнопка с именем органа управления и список всех органов.

// Нажали кнопку - она ждёт следующего нажатия клавиши, кнопки мыши или

// геймпада или отклонения стика и запоминает его; Escape отменяет. Оси мыши

// нажатием не задать - их выбирают из списка. Какое поле ждёт, хранится в

// памяти окна ImGui.

} // namespace

bool inputControlField(std::string& value, const Input& input)
{
    ImGuiStorage& storage = *ImGui::GetStateStorage();
    const ImGuiID waitingKey = ImGui::GetID(kWaitingForControl);
    const bool waiting = storage.GetBool(waitingKey, false);
    bool changed = false;

    const float listWidth = ImGui::GetFrameHeight();
    std::string label = waiting         ? tr("editor.inspector.pressControl")
                        : value.empty() ? tr("editor.inspector.none")
                                        : value;
    label += "##control";
    if (ImGui::Button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - listWidth, 0.0f))) {
        storage.SetBool(waitingKey, !waiting);
    }
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::SetNextItemWidth(listWidth);
    if (ImGui::BeginCombo("##controls", nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft)) {
        for (const InputControl& control : allInputControls()) {
            const std::string name(controlName(control));
            if (ImGui::Selectable(name.c_str(), name == value)) {
                value = name;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    if (!waiting) {
        return changed;
    }
    if (input.wasKeyPressed(Key::Escape)) {
        storage.SetBool(waitingKey, false);

        return changed;
    }
    if (const auto activated = activatedControl(input)) {
        storage.SetBool(waitingKey, false);
        value = std::string(controlName(*activated));
        changed = true;
    }

    return changed;
}

} // namespace yqlora::editor
