#include "yqlora/render/OutputPass.hpp"

#include "yqlora/render/FrameFormats.hpp"
#include "yqlora/render/ViewPass.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"
#include "yqlora/shader/ShaderProfile.hpp"

namespace yqlora {

namespace {

// Один треугольник на весь экран: вершины строит сам шейдер по номеру вершины.
constexpr std::uint32_t kFullscreenTriangleVertices = 3;

constexpr rhi::BindingSlot kHdrTexture{kMaterialSet, 0};

} // namespace

OutputPass::OutputPass(const RenderContext& context)
    : m_shader(context.device.createShader(buildBuiltinShader(
          context.shaders,
          {
              .name = "output",
              .vertexPath = engineShaderPath("fullscreen.vert"),
              .fragmentPath = engineShaderPath("output.frag"),
          }
      )))
    , m_pipeline(context.device.createPipeline({
          .shader = m_shader.get(),
          .depthTest = false,
          .depthWrite = false,
          .cull = rhi::CullMode::None,
          .colorFormat = kOutputColorFormat,
          .depthFormat = rhi::Format::None,
      }))
    , m_sampler(
          context.device.createSampler({.filter = rhi::Filter::Nearest, .wrap = rhi::Wrap::Clamp, .mipmaps = false})
      )
{
}

void OutputPass::addTo(RenderGraph& graph, RenderGraph::TextureId hdr, RenderGraph::TextureId target) const
{
    graph.addPass({
        .name = "output",
        // Весь кадр перезаписывается, стирать незачем.
        .color = {target, rhi::LoadOp::Load},
        .reads = {hdr},
        .record = [this, hdr](rhi::CommandList& commands, const RenderGraph& passes) {
            commands.bindPipeline(*m_pipeline);
            commands.setTexture(kHdrTexture.set, kHdrTexture.binding, passes.texture(hdr), *m_sampler);
            commands.draw(kFullscreenTriangleVertices);
        },
    });
}

} // namespace yqlora
