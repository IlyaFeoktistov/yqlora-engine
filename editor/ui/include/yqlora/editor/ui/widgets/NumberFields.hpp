#pragma once

#include "yqlora/editor/ui/widgets/PropertyEditor.hpp"

#include <imgui.h>

namespace yqlora::editor {

struct EditorColors;

// Ключ памяти окна ImGui: число вектора, по которому кликнули правой кнопкой
// в этом кадре (vectorFields); его читает меню строки свойства.
inline constexpr const char* kClickedComponent = "##clickedComponent";
inline constexpr const char* kComponentNames[] = {"X", "Y", "Z", "W"};

// Подпись, которую можно тянуть мышью, как в Unity: Shift ускоряет в 10 раз,
// Ctrl замедляет. Меняет value и отмечает в result, что значение тянут.
void dragLabel(
    const char* id,
    const char* text,
    const ImVec4& color,
    float& value,
    float speed,
    PropertyResult& result
);

// Поле числа без подписи заданной ширины. format влияет только на показ:
// введённое значение сохраняется с полной точностью.
void floatField(const char* id, float& value, float width, const char* format, PropertyResult& result);

// Числа вектора в одну строку, у каждого своя цветная подпись.
void vectorFields(float* values, int count, float speed, const EditorColors& colors, PropertyResult& result);

} // namespace yqlora::editor
