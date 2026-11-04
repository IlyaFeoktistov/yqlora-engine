#include "yqlora/render/EngineBindings.hpp"

#include "yqlora/render/EngineUniforms.hpp"

namespace yqlora {

namespace {

void bindBlock(
    rhi::CommandList& commands,
    const rhi::Shader& shader,
    const UniformLayout& block,
    const UniformSlice& slice
)
{
    if (slice.buffer && shader.findBinding(block.slot)) {
        setUniforms(commands, block, slice);
    }
}

void bindTexture(
    rhi::CommandList& commands,
    const rhi::Shader& shader,
    rhi::BindingSlot slot,
    const rhi::Texture* texture,
    const rhi::Sampler* sampler
)
{
    if (texture && sampler && shader.findBinding(slot)) {
        commands.setTexture(slot.set, slot.binding, *texture, *sampler);
    }
}

} // namespace

void bindEngineResources(rhi::CommandList& commands, const rhi::Shader& shader, const EngineResources& resources)
{
    bindBlock(commands, shader, kFrameBlock, resources.frame);
    bindBlock(commands, shader, kAtmosphereBlock, resources.atmosphere);
    bindBlock(commands, shader, kViewBlock, resources.view);
    bindTexture(commands, shader, kTransmittanceLutSlot, resources.transmittance, resources.lutSampler);
    bindTexture(commands, shader, kSkyViewLutSlot, resources.skyView, resources.lutSampler);
    bindTexture(commands, shader, kAerialPerspectiveLutSlot, resources.aerialPerspective, resources.lutSampler);
}

} // namespace yqlora
