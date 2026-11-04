#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/shader/ShaderCompiler.hpp"
#include "yqlora/shader/ShaderData.hpp"
#include "yqlora/shader/UniformLayout.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace yqlora {

struct ShaderDataResult {
    std::optional<ShaderData> data{};
    // Все ошибки компиляции, профиля, блоков и пометок, с файлом и строкой.
    std::vector<ShaderMessage> errors{};
    // Подключённые файлы, и при ошибке тоже.
    std::vector<std::string> includes{};
};

// Шейдер из одного файла path (обе стадии) в ресурс: компиляция, профиль,
// сверка блоков движка layouts, правила набора материала (блок Material на
// привязке 0, остальное - текстуры) и пометки файла. Шейдер, подключивший
// <yqlora/sky.glsl>, - шейдер неба: он собирается ещё и вариантом захвата.
YQLORA_API ShaderDataResult buildShaderData(
    ShaderCompiler& compiler,
    const std::string& path,
    std::string name,
    std::span<const UniformLayout* const> layouts
);

} // namespace yqlora
