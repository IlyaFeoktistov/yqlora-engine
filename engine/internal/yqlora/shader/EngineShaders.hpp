#pragma once

#include "yqlora/core/Export.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace yqlora {

// Шейдеры движка вшиты в библиотеку (engine/shaders). Путь к ним в
// компиляторе начинается с этой приставки: "engine:mesh.vert",
// "engine:include/yqlora/uniforms.glsl"; ошибки показывают его же.
inline constexpr std::string_view kEngineShaderScheme = "engine:";
// #include <yqlora/...> ищется здесь.
inline constexpr std::string_view kEngineShaderIncludeDir = "include/";

// Шейдер, подключивший этот файл, - шейдер неба (ShaderKind::Sky).
inline constexpr std::string_view kSkyShaderInclude = "include/yqlora/sky.glsl";

// Файл из engine/shaders по пути без приставки; nullopt - такого нет.
YQLORA_API std::optional<std::string_view> engineShaderFile(std::string_view path);

inline std::string engineShaderPath(std::string_view path)
{
    return std::string(kEngineShaderScheme) + std::string(path);
}

} // namespace yqlora
