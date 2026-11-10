#pragma once

#include "yqlora/math/Math.hpp"

#include <imgui.h>

#include <filesystem>

namespace yqlora::editor {

// Цвета редактора, которых нет среди цветов ImGui.
struct EditorColors {
    Vec4 errorText{1.0f, 0.45f, 0.4f, 1.0f};
    Vec4 warningText{0.9f, 0.75f, 0.48f, 1.0f};
    Vec4 selectionOutline{1.0f, 0.6f, 0.1f, 1.0f};
    // Контур камер в окне сцены.
    Vec4 cameraGizmo{0.78f, 0.8f, 0.84f, 1.0f};
    // Значки и очертания источников света в окне сцены.
    Vec4 lightGizmo{0.95f, 0.82f, 0.42f, 1.0f};
    // Фон окна сцены, когда смотрим через камеру редактора.
    Vec4 viewportBackground{0.08f, 0.09f, 0.12f, 1.0f};
    // Рамка окна сцены и кнопка Play, пока идёт игра.
    Vec4 playMode{0.37f, 0.72f, 0.47f, 1.0f};
    // Подписи X, Y, Z, W у полей векторов.
    Vec4 axisX{0.9f, 0.35f, 0.33f, 1.0f};
    Vec4 axisY{0.45f, 0.75f, 0.35f, 1.0f};
    Vec4 axisZ{0.35f, 0.55f, 0.95f, 1.0f};
    Vec4 axisW{0.6f, 0.6f, 0.6f, 1.0f};
    // Узлы экземпляров префабов в дереве сцены.
    Vec4 prefabInstance{0.42f, 0.71f, 0.93f, 1.0f};
    // Подписи переопределённых в экземпляре свойств и компонентов.
    Vec4 prefabOverride{0.94f, 0.72f, 0.35f, 1.0f};
};

// Размеры элементов редактора, которых нет среди настроек ImGui.
struct EditorMetrics {
    // Отступ вокруг флажка по вертикали: флажок меньше поля ввода.
    float checkboxPadding = 2.0f;
};

// Внешний вид интерфейса задаётся файлом темы (assets/themes/*.json), а не кодом:
// шрифт, размеры и отступы, цвета ImGui и цвета редактора. Чего нет в файле,
// остаётся как в стандартной тёмной теме ImGui. Принадлежит EditorApp,
// панелям доступна через EditorContext.
class Theme {
public:
    // Применяет тему к ImGui. Вызывать до первого кадра интерфейса: шрифт
    // добавляется в атлас ImGui. assetsDir - папка, от которой считается путь к шрифту.
    bool load(const std::filesystem::path& file, const std::filesystem::path& assetsDir);

    const EditorColors& colors() const
    {
        return m_colors;
    }

    const EditorMetrics& metrics() const
    {
        return m_metrics;
    }

private:
    EditorColors m_colors;
    EditorMetrics m_metrics;
};

// Цвет темы в виде, который принимает ImGui. alpha умножается на прозрачность цвета.
inline ImVec4 toImVec4(const Vec4& color, float alpha = 1.0f)
{
    return {color.r, color.g, color.b, color.a * alpha};
}

inline ImU32 toImU32(const Vec4& color, float alpha = 1.0f)
{
    return ImGui::GetColorU32(toImVec4(color, alpha));
}

} // namespace yqlora::editor
