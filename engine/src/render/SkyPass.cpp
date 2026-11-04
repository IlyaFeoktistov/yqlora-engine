#include "yqlora/render/SkyPass.hpp"

#include "yqlora/render/FrameFormats.hpp"
#include "yqlora/render/MaterialBinding.hpp"
#include "yqlora/render/PipelineCache.hpp"
#include "yqlora/render/ViewPass.hpp"

namespace yqlora {

namespace {

// Треугольник на весь экран строит сам шейдер (sky.glsl).
constexpr std::uint32_t kFullscreenTriangleVertices = 3;

} // namespace

void drawSky(
    rhi::CommandList& commands,
    const RenderContext& context,
    const FrameData& frame,
    const EngineResources& resources
)
{
    if (!frame.environment.sky) {
        return;
    }
    const FrameMaterial& material = frame.materials[*frame.environment.sky];
    const rhi::Pipeline* pipeline = context.pipelines.get(material, MaterialPass::Sky, kHdrColorFormat, kDepthFormat);
    if (!pipeline) {
        return;
    }
    commands.bindPipeline(*pipeline);
    bindEngineResources(commands, *pipeline->desc().shader, resources);
    bindMaterial(commands, context, *pipeline->desc().shader, material, nullptr);
    commands.draw(kFullscreenTriangleVertices);
}

} // namespace yqlora
