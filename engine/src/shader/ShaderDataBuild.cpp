#include "yqlora/shader/ShaderDataBuild.hpp"

#include "yqlora/shader/MaterialAnnotations.hpp"
#include "yqlora/shader/ShaderAnnotations.hpp"
#include "yqlora/shader/ShaderBuild.hpp"
#include "yqlora/shader/ShaderProfile.hpp"

#include <algorithm>

namespace yqlora {

namespace {

// Набор материала: блок Material на привязке 0, остальное - текстуры.
void collectMaterial(const rhi::ShaderReflection& reflection, ShaderData& data, std::vector<std::string>& errors)
{
    for (const rhi::ShaderBinding& binding : reflection.bindings) {
        if (binding.slot.set != kMaterialSet) {
            continue;
        }
        if (binding.kind == rhi::BindingKind::Texture) {
            data.textures.push_back({.name = binding.name, .slot = binding.slot, .dimension = binding.dimension});
            continue;
        }
        if (binding.slot != kMaterialBlockSlot || binding.name != kMaterialBlockName) {
            errors.push_back(
                "uniform block '" + binding.name
                + "': the material set holds only the Material block at binding 0 and textures"
            );
            continue;
        }
        data.materialSize = binding.size;
        for (const rhi::UniformField& field : binding.fields) {
            data.fields.push_back({
                .name = field.name,
                .type = field.type,
                .offset = field.offset,
                .size = field.size,
                // Массив материал не задаёт: у него нет значения по умолчанию.
                .defaultValue = std::vector<float>(field.arraySize == 0 ? rhi::componentCount(field.type) : 0, 0.0f),
            });
        }
    }
    std::ranges::sort(data.textures, {}, [](const MaterialTexture& texture) {
        return texture.slot.binding;
    });
}

} // namespace

ShaderDataResult buildShaderData(
    ShaderCompiler& compiler,
    const std::string& path,
    std::string name,
    std::span<const UniformLayout* const> layouts
)
{
    ShaderDataResult result;
    ShaderBuildResult built = buildShader(
        compiler,
        {.name = std::move(name), .vertexPath = path, .fragmentPath = path, .layouts = layouts}
    );
    result.includes = std::move(built.includes);
    result.errors = std::move(built.errors);
    if (!built.desc) {
        return result;
    }

    ShaderData data;
    std::vector<std::string> materialErrors;
    collectMaterial(built.desc->reflection, data, materialErrors);
    for (std::string& text : materialErrors) {
        result.errors.push_back({.file = path, .line = 0, .text = std::move(text)});
    }

    // Файл только что скомпилировался, поэтому он есть.
    const AnnotationParseResult parsed = parseShaderAnnotations(compiler.read(path).value_or(""), path);
    result.errors.insert(result.errors.end(), parsed.errors.begin(), parsed.errors.end());
    for (const AnnotatedDeclaration& declaration : parsed.declarations) {
        // Объявление, которым шейдер не пользуется, в отражение не попадает:
        // его пометкам нечего описывать.
        if (declaration.texture) {
            if (const auto it = std::ranges::find(data.textures, declaration.name, &MaterialTexture::name);
                it != data.textures.end()) {
                applyAnnotations(declaration, *it, path, result.errors);
            }
        } else if (
            const auto it = std::ranges::find(data.fields, declaration.name, &MaterialField::name);
            it != data.fields.end()
        ) {
            applyAnnotations(declaration, *it, path, result.errors);
        }
    }
    if (result.errors.empty()) {
        data.desc = std::move(*built.desc);
        result.data = std::move(data);
    }

    return result;
}

} // namespace yqlora
