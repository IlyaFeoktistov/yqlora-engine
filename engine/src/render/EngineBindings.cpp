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

std::vector<RenderGraph::TextureId> EngineTextureIds::reads() const
{
    std::vector<RenderGraph::TextureId> result;
    for (const auto& id :
         {transmittance,
          skyView,
          aerialPerspective,
          ambientHarmonics,
          environmentSpecular,
          environmentBrdf,
          shadowCascades,
          shadowAtlas}) {
        if (id) {
            result.push_back(*id);
        }
    }

    return result;
}

EngineResources withTextures(EngineResources resources, const RenderGraph& graph, const EngineTextureIds& ids)
{
    const auto texture = [&](const std::optional<RenderGraph::TextureId>& id) -> const rhi::Texture* {
        return id ? &graph.texture(*id) : nullptr;
    };
    resources.transmittance = texture(ids.transmittance);
    resources.skyView = texture(ids.skyView);
    resources.aerialPerspective = texture(ids.aerialPerspective);
    resources.ambientHarmonics = texture(ids.ambientHarmonics);
    resources.environmentSpecular = texture(ids.environmentSpecular);
    resources.environmentBrdf = texture(ids.environmentBrdf);
    resources.shadowCascades = texture(ids.shadowCascades);
    resources.shadowAtlas = texture(ids.shadowAtlas);

    return resources;
}

void bindEngineResources(rhi::CommandList& commands, const rhi::Shader& shader, const EngineResources& resources)
{
    bindBlock(commands, shader, kFrameBlock, resources.frame);
    bindBlock(commands, shader, kAtmosphereBlock, resources.atmosphere);
    bindBlock(commands, shader, kViewBlock, resources.view);
    bindTexture(commands, shader, kTransmittanceLutSlot, resources.transmittance, resources.lutSampler);
    bindTexture(commands, shader, kSkyViewLutSlot, resources.skyView, resources.lutSampler);
    bindTexture(commands, shader, kAerialPerspectiveLutSlot, resources.aerialPerspective, resources.lutSampler);
    bindTexture(commands, shader, kAmbientHarmonicsSlot, resources.ambientHarmonics, resources.environmentSampler);
    bindTexture(
        commands,
        shader,
        kEnvironmentSpecularSlot,
        resources.environmentSpecular,
        resources.environmentSampler
    );
    bindTexture(commands, shader, kEnvironmentBrdfSlot, resources.environmentBrdf, resources.environmentSampler);
    bindBlock(commands, shader, kShadowBlock, resources.shadows);
    bindTexture(commands, shader, kShadowCascadesSlot, resources.shadowCascades, resources.shadowCompareSampler);
    bindTexture(commands, shader, kShadowCascadesDepthSlot, resources.shadowCascades, resources.shadowDepthSampler);
    bindTexture(commands, shader, kShadowAtlasSlot, resources.shadowAtlas, resources.shadowCompareSampler);
    bindTexture(commands, shader, kShadowAtlasDepthSlot, resources.shadowAtlas, resources.shadowDepthSampler);
}

} // namespace yqlora
