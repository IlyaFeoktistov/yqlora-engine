#include "yqlora/render/MaterialBinding.hpp"

#include "yqlora/render/GpuMaterial.hpp"
#include "yqlora/render/GpuTexture.hpp"
#include "yqlora/render/MaterialBlock.hpp"
#include "yqlora/render/ViewPass.hpp"
#include "yqlora/shader/ShaderProfile.hpp"

#include <span>

namespace yqlora {

void bindMaterial(
    rhi::CommandList& commands,
    const RenderContext& context,
    const rhi::Shader& shader,
    const FrameMaterial& material,
    const FrameOverrides* overrides,
    const std::function<void(std::string_view)>& misfit
)
{
    if (!shader.findBinding(kMaterialBlockSlot)) {
        // Блока нет в этом варианте шейдера: значения ему не нужны.
    } else if (overrides && material.shader->materialSize != 0) {
        // Замены - свои данные материала у этого элемента на этот кадр.
        const auto block = materialBlock(*material.shader, *material.data, &overrides->values, misfit);
        const UniformSlice slice = context.uniforms.push(std::span<const std::byte>(block));
        commands.setUniformBuffer(kMaterialSet, kMaterialBlockSlot.binding, *slice.buffer, slice.offset, slice.size);
    } else if (const rhi::Buffer* block = context.materials.block(material)) {
        commands.setUniformBuffer(kMaterialSet, kMaterialBlockSlot.binding, *block, 0, block->desc().size);
    }
    for (const FrameTexture& texture : overrides ? overrides->textures : material.textures) {
        if (!shader.findBinding(texture.slot)) {
            continue;
        }
        commands.setTexture(
            texture.slot.set,
            texture.slot.binding,
            context.textures.texture(texture),
            context.textures.sampler(texture)
        );
    }
}

} // namespace yqlora
