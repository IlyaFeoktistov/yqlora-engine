#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/Pipeline.hpp"
#include "yqlora/shader/ShaderCompiler.hpp"
#include "yqlora/shader/UniformLayout.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace yqlora {

// Определения, которые получает каждая стадия: по ним одна стадия файла
// .yshader отличается от другой (#ifdef YQLORA_VERTEX).
inline constexpr char kVertexStageDefine[] = "YQLORA_VERTEX";
inline constexpr char kFragmentStageDefine[] = "YQLORA_FRAGMENT";

struct ShaderBuildRequest {
    std::string name{};
    // У файла .yshader оба пути одинаковые.
    std::string vertexPath{};
    std::string fragmentPath{};
    std::vector<ShaderDefine> defines{};
    // Блоки, которые шейдер делит со структурами C++ (кадр, вид, объект и
    // свои у прохода): сверяются с отражением.
    std::span<const UniformLayout* const> layouts{};
};

struct ShaderBuildResult {
    std::optional<rhi::ShaderDesc> desc{};
    std::vector<ShaderMessage> errors{};
    // Подключённые файлы обеих стадий, каждый один раз.
    std::vector<std::string> includes{};
};

// Весь путь шейдера до устройства: компиляция обеих стадий, отражение,
// профиль, сверка блоков с C++. Ошибки всех шагов вместе.
YQLORA_API ShaderBuildResult buildShader(ShaderCompiler& compiler, const ShaderBuildRequest& request);

// То же для встроенных шейдеров (движка, редактора): их ошибка - ошибка
// сборки программы, поэтому std::runtime_error со всеми сообщениями.
YQLORA_API rhi::ShaderDesc buildBuiltinShader(ShaderCompiler& compiler, const ShaderBuildRequest& request);

} // namespace yqlora
