#include "yqlora/opengl/GlslTranslator.hpp"

#include <spirv_glsl.hpp>

#include <stdexcept>

namespace yqlora::opengl {

namespace {

constexpr std::uint32_t kGlslVersion = 330;

std::string varyingName(std::uint32_t location)
{
    return "yq_varying_" + std::to_string(location);
}

} // namespace

std::string glResourceName(rhi::BindingSlot slot)
{
    return "yq_set" + std::to_string(slot.set) + "_binding" + std::to_string(slot.binding);
}

std::string translateToGlsl330(std::span<const std::uint32_t> spirv, rhi::ShaderStage stage)
{
    try {
        spirv_cross::CompilerGLSL compiler(spirv.data(), spirv.size());

        spirv_cross::CompilerGLSL::Options options;
        options.version = kGlslVersion;
        options.es = false;
        // Привязки в шейдере потребовали бы расширения, которого нет у
        // OpenGL 3.3: места ресурсам назначает GlShader после линковки.
        options.enable_420pack_extension = false;
        options.vertex.fixup_clipspace = stage == rhi::ShaderStage::Vertex;
        // Без него SPIRV-Cross добавит uniform для gl_InstanceIndex.
        options.vertex.support_nonzero_base_instance = false;
        compiler.set_common_options(options);

        const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
        const auto rename = [&](const spirv_cross::Resource& resource, spirv_cross::ID named) {
            const rhi::BindingSlot slot{
                compiler.get_decoration(resource.id, spv::DecorationDescriptorSet),
                compiler.get_decoration(resource.id, spv::DecorationBinding),
            };
            compiler.set_name(named, glResourceName(slot));
            compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
            compiler.unset_decoration(resource.id, spv::DecorationBinding);
        };
        // У блока OpenGL ищет имя типа блока, у текстуры - имя переменной.
        for (const spirv_cross::Resource& resource : resources.uniform_buffers) {
            rename(resource, resource.base_type_id);
        }
        for (const spirv_cross::Resource& resource : resources.sampled_images) {
            rename(resource, resource.id);
        }
        // Входы вершин сохраняют места: layout(location) для них в 330 есть.
        if (stage == rhi::ShaderStage::Fragment) {
            for (const spirv_cross::Resource& resource : resources.stage_inputs) {
                compiler.set_name(
                    resource.id,
                    varyingName(compiler.get_decoration(resource.id, spv::DecorationLocation))
                );
                compiler.unset_decoration(resource.id, spv::DecorationLocation);
            }
        } else {
            for (const spirv_cross::Resource& resource : resources.stage_outputs) {
                compiler.set_name(
                    resource.id,
                    varyingName(compiler.get_decoration(resource.id, spv::DecorationLocation))
                );
                compiler.unset_decoration(resource.id, spv::DecorationLocation);
            }
        }

        return compiler.compile();
    } catch (const spirv_cross::CompilerError& error) {
        throw std::runtime_error(std::string("SPIR-V to GLSL 330 failed: ") + error.what());
    }
}

} // namespace yqlora::opengl
