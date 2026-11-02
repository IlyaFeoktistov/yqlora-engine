#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace yqlora::editor {

// Шейдеры редактора вшиты в его библиотеку (editor/shaders). Путь к ним в
// компиляторе - "editor/<файл>"; #include "x.glsl" ищется там же,
// <yqlora/...> - в библиотеке движка.
inline constexpr std::string_view kEditorShaderDir = "editor/";

inline std::string editorShaderPath(std::string_view file)
{
    return std::string(kEditorShaderDir) + std::string(file);
}

// ShaderFileReader для компилятора шейдеров.
std::optional<std::string> readEditorShader(const std::string& path);

} // namespace yqlora::editor
