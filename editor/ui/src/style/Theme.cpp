#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/core/JsonFile.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"

#include <imgui.h>
// ImGuizmo.h использует типы ImGui и должен идти после imgui.h.
#include <ImGuizmo.h>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace yqlora::editor {

namespace {

// Цвет в теме: "#RRGGBB" или "#RRGGBBAA", по две шестнадцатеричные цифры на канал.
constexpr std::size_t kRgbDigits = 6;
constexpr std::size_t kRgbaDigits = 8;
constexpr std::size_t kDigitsPerChannel = 2;
constexpr int kHexBase = 16;
constexpr float kMaxChannel = 255.0f;
constexpr float kDefaultFontSize = 17.0f;
// Плоскости манипулятора полупрозрачные, чтобы не закрывать объект.
const Vec4 kPlaneTint(1.0f, 1.0f, 1.0f, 0.4f);

using Json = nlohmann::json;

// "#RRGGBB" или "#RRGGBBAA".
std::optional<Vec4> parseColor(const Json& json)
{
    if (!json.is_string()) {
        return std::nullopt;
    }
    const std::string text = json.get<std::string>();
    const std::size_t digits = text.size() - 1;
    if (text.empty() || text.front() != '#' || (digits != kRgbDigits && digits != kRgbaDigits)) {
        return std::nullopt;
    }
    float channels[] = {0.0f, 0.0f, 0.0f, kMaxChannel};
    for (std::size_t i = 0; i < digits / kDigitsPerChannel; ++i) {
        try {
            const std::string channel = text.substr(1 + i * kDigitsPerChannel, kDigitsPerChannel);
            channels[i] = static_cast<float>(std::stoul(channel, nullptr, kHexBase));
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    const Vec4 bytes(channels[0], channels[1], channels[2], channels[3]);

    return bytes / kMaxChannel;
}

void applyColors(const Json& json, ImGuiStyle& style)
{
    if (!json.is_object()) {
        return;
    }
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        const auto it = json.find(ImGui::GetStyleColorName(i));
        if (it == json.end()) {
            continue;
        }
        if (const auto color = parseColor(*it)) {
            style.Colors[i] = ImVec4(color->r, color->g, color->b, color->a);
        } else {
            log::warn("Theme: color '{}' is not #RRGGBB or #RRGGBBAA", it.key());
        }
    }
}

void applyStyle(const Json& json, ImGuiStyle& style)
{
    if (!json.is_object()) {
        return;
    }

    struct FloatField {
        std::string_view name;
        float ImGuiStyle::* field;
    };
    struct VectorField {
        std::string_view name;
        ImVec2 ImGuiStyle::* field;
    };
    static constexpr FloatField kFloats[] = {
        {"indentSpacing", &ImGuiStyle::IndentSpacing},
        {"scrollbarSize", &ImGuiStyle::ScrollbarSize},
        {"windowRounding", &ImGuiStyle::WindowRounding},
        {"childRounding", &ImGuiStyle::ChildRounding},
        {"frameRounding", &ImGuiStyle::FrameRounding},
        {"popupRounding", &ImGuiStyle::PopupRounding},
        {"scrollbarRounding", &ImGuiStyle::ScrollbarRounding},
        {"grabRounding", &ImGuiStyle::GrabRounding},
        {"tabRounding", &ImGuiStyle::TabRounding},
        {"windowBorderSize", &ImGuiStyle::WindowBorderSize},
        {"frameBorderSize", &ImGuiStyle::FrameBorderSize},
        {"popupBorderSize", &ImGuiStyle::PopupBorderSize},
    };
    static constexpr VectorField kVectors[] = {
        {"windowPadding", &ImGuiStyle::WindowPadding},
        {"framePadding", &ImGuiStyle::FramePadding},
        {"itemSpacing", &ImGuiStyle::ItemSpacing},
        {"itemInnerSpacing", &ImGuiStyle::ItemInnerSpacing},
    };

    for (const auto& [name, field] : kFloats) {
        if (const auto it = json.find(name); it != json.end() && it->is_number()) {
            style.*field = it->get<float>();
        }
    }
    for (const auto& [name, field] : kVectors) {
        const auto it = json.find(name);
        if (it != json.end() && it->is_array() && it->size() == 2 && (*it)[0].is_number() && (*it)[1].is_number()) {
            style.*field = ImVec2((*it)[0].get<float>(), (*it)[1].get<float>());
        }
    }

    // Кнопка со списком вкладок в углу пристыкованного окна: "left", "right" или "none".
    if (const auto it = json.find("windowMenuButtonPosition"); it != json.end() && it->is_string()) {
        const std::string position = it->get<std::string>();
        if (position == "left") {
            style.WindowMenuButtonPosition = ImGuiDir_Left;
        } else if (position == "right") {
            style.WindowMenuButtonPosition = ImGuiDir_Right;
        } else if (position == "none") {
            style.WindowMenuButtonPosition = ImGuiDir_None;
        } else {
            log::warn("Theme: windowMenuButtonPosition '{}' is not left, right or none", position);
        }
    }
}

// Шрифт из описания {"file": ..., "size": ...}. merge - подмешать к предыдущему
// шрифту: так иконки встают прямо в текст.
void addFont(const Json& json, const std::filesystem::path& assetsDir, bool merge)
{
    if (!json.is_object() || !json.contains("file") || !json["file"].is_string()) {
        return;
    }
    const std::string file = json["file"].get<std::string>();
    const float size = json.contains("size") && json["size"].is_number() ? json["size"].get<float>() : kDefaultFontSize;
    const auto path = assetsDir / fromUtf8(file);

    ImFontConfig config;
    config.MergeMode = merge;
    if (const auto it = json.find("offsetY"); it != json.end() && it->is_number()) {
        config.GlyphOffset.y = it->get<float>();
    }
    // Глифы подгружаются по мере надобности, кириллица в том числе.
    if (!ImGui::GetIO().Fonts->AddFontFromFileTTF(toUtf8(path).c_str(), size, &config)) {
        log::warn("Theme: font '{}' not found", toUtf8(path));
    }
}

void applyEditorColors(const Json& json, EditorColors& colors)
{
    if (!json.is_object()) {
        return;
    }
    const auto read = [&](const char* name, Vec4& out) {
        if (const auto it = json.find(name); it != json.end()) {
            if (const auto color = parseColor(*it)) {
                out = *color;
            }
        }
    };
    read("errorText", colors.errorText);
    read("warningText", colors.warningText);
    read("selectionOutline", colors.selectionOutline);
    read("cameraGizmo", colors.cameraGizmo);
    read("lightGizmo", colors.lightGizmo);
    read("viewportBackground", colors.viewportBackground);
    read("playMode", colors.playMode);
    read("axisX", colors.axisX);
    read("axisY", colors.axisY);
    read("axisZ", colors.axisZ);
    read("axisW", colors.axisW);
}

void applyEditorMetrics(const Json& json, EditorMetrics& metrics)
{
    if (!json.is_object()) {
        return;
    }
    if (const auto it = json.find("checkboxPadding"); it != json.end() && it->is_number()) {
        metrics.checkboxPadding = it->get<float>();
    }
}

// Манипуляторы окрашены в те же цвета осей, что и поля X, Y, Z в инспекторе.
void applyManipulatorColors(const EditorColors& colors)
{
    ImGuizmo::Style& style = ImGuizmo::GetStyle();
    const auto set = [&](ImGuizmo::COLOR slot, const Vec4& color) {
        style.Colors[slot] = toImVec4(color);
    };
    set(ImGuizmo::DIRECTION_X, colors.axisX);
    set(ImGuizmo::DIRECTION_Y, colors.axisY);
    set(ImGuizmo::DIRECTION_Z, colors.axisZ);
    set(ImGuizmo::PLANE_X, colors.axisX * kPlaneTint);
    set(ImGuizmo::PLANE_Y, colors.axisY * kPlaneTint);
    set(ImGuizmo::PLANE_Z, colors.axisZ * kPlaneTint);
    set(ImGuizmo::SELECTION, colors.selectionOutline);
}

} // namespace

bool Theme::load(const std::filesystem::path& file, const std::filesystem::path& assetsDir)
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);

    const Json json = readJsonFile(file).value_or(Json());
    if (!json.is_object()) {
        log::warn("Theme '{}' is not found or broken, using the default look", toUtf8(file));

        return false;
    }

    addFont(json.value("font", Json()), assetsDir, false);
    addFont(json.value("iconFont", Json()), assetsDir, true);
    applyStyle(json.value("style", Json()), style);
    applyColors(json.value("colors", Json()), style);
    applyEditorColors(json.value("editor", Json()), m_colors);
    applyEditorMetrics(json.value("editorStyle", Json()), m_metrics);
    applyManipulatorColors(m_colors);

    return true;
}

} // namespace yqlora::editor
