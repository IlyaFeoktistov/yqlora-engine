#pragma once

#include "yqlora/reflection/EnumNames.hpp"

#include <imgui.h>

#include <cstddef>
#include <iterator>
#include <string>

namespace yqlora::editor {

// Выбор значения перечисления по его таблице имён (EnumNames). true, если
// выбрали другое.
template <NamedEnum E>
bool enumCombo(const char* label, E& value)
{
    bool changed = false;
    if (ImGui::BeginCombo(label, std::string(enumName(value)).c_str())) {
        for (std::size_t i = 0; i < std::size(EnumNames<E>::names); ++i) {
            const auto option = static_cast<E>(i);
            if (ImGui::Selectable(std::string(EnumNames<E>::names[i]).c_str(), option == value) && option != value) {
                value = option;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}

} // namespace yqlora::editor
