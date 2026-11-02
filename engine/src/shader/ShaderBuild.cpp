#include "yqlora/shader/ShaderBuild.hpp"

#include "yqlora/shader/ShaderProfile.hpp"

#include <stdexcept>

namespace yqlora {

namespace {

void append(std::vector<ShaderMessage>& errors, const std::vector<std::string>& texts, const std::string& file)
{
    for (const std::string& text : texts) {
        errors.push_back({.file = file, .line = 0, .text = text});
    }
}

} // namespace

ShaderBuildResult buildShader(ShaderCompiler& compiler, const ShaderBuildRequest& request)
{
    ShaderBuildResult result;
    ShaderCompileResult vertex = compiler.compile({
        .path = request.vertexPath,
        .stage = rhi::ShaderStage::Vertex,
        .defines = request.defines,
    });
    ShaderCompileResult fragment = compiler.compile({
        .path = request.fragmentPath,
        .stage = rhi::ShaderStage::Fragment,
        .defines = request.defines,
    });
    result.errors = vertex.errors;
    result.errors.insert(result.errors.end(), fragment.errors.begin(), fragment.errors.end());
    if (!result.errors.empty()) {
        return result;
    }

    ShaderReflectResult reflected = reflectShader(vertex.spirv, fragment.spirv);
    // Ошибки отражения и профиля относятся к шейдеру целиком, а не к строке.
    const std::string& file = request.vertexPath;
    append(result.errors, reflected.errors, file);
    append(result.errors, checkShaderProfile(reflected.reflection), file);
    for (const UniformLayout* layout : request.layouts) {
        append(result.errors, checkUniformLayout(reflected.reflection, *layout), file);
    }
    if (!result.errors.empty()) {
        return result;
    }

    result.desc = rhi::ShaderDesc{
        .name = request.name,
        .vertex = std::move(vertex.spirv),
        .fragment = std::move(fragment.spirv),
        .reflection = std::move(reflected.reflection),
    };

    return result;
}

rhi::ShaderDesc buildEngineShader(ShaderCompiler& compiler, const ShaderBuildRequest& request)
{
    ShaderBuildResult result = buildShader(compiler, request);
    if (!result.desc) {
        std::string text = "Engine shader '" + request.name + "' failed:";
        for (const ShaderMessage& message : result.errors) {
            text += "\n" + formatShaderMessage(message);
        }
        throw std::runtime_error(text);
    }

    return std::move(*result.desc);
}

} // namespace yqlora
