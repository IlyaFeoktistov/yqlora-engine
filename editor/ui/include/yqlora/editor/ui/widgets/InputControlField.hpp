#pragma once

#include <string>

namespace yqlora {
class Input;
} // namespace yqlora

namespace yqlora::editor {

// Привязка действия (PropertyHint::InputControlName): кнопка с именем органа
// управления и список всех органов. Нажали кнопку - она ждёт следующего
// нажатия клавиши, кнопки мыши или геймпада или отклонения стика и
// запоминает его; Escape отменяет. Оси мыши нажатием не задать - их выбирают
// из списка. Какое поле ждёт, хранится в памяти окна ImGui.
bool inputControlField(std::string& value, const Input& input);

} // namespace yqlora::editor
