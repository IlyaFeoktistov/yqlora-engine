#include "yqlora/shader/BuiltinShaders.hpp"

#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/resource/BuiltinResources.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderDataBuild.hpp"

#include <stdexcept>

namespace yqlora {

std::span<const BuiltinShader> builtinShaders()
{
    static const BuiltinShader shaders[] = {
        {builtin::kStandardShader, "Standard", "standard.yshader"},
        {builtin::kErrorShader, "Error", "error.yshader"},
    };

    return shaders;
}

ShaderData makeBuiltinShader(const BuiltinShader& shader)
{
    // Файлов проекта встроенным шейдерам читать не нужно.
    ShaderCompiler compiler(ShaderFileReader{});
    ShaderDataResult
        result = buildShaderData(compiler, engineShaderPath(shader.file), std::string(shader.name), kEngineBlocks);
    if (!result.data) {
        std::string text = "Built-in shader '" + std::string(shader.name) + "' failed:";
        for (const ShaderMessage& message : result.errors) {
            text += "\n" + formatShaderMessage(message);
        }
        throw std::runtime_error(text);
    }

    return std::move(*result.data);
}

} // namespace yqlora
