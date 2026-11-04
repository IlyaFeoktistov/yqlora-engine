#include "yqlora/render/OutputPass.hpp"

#include "yqlora/render/FrameFormats.hpp"
#include "yqlora/render/ViewPass.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"
#include "yqlora/shader/ShaderProfile.hpp"

#include <string>
#include <string_view>

namespace yqlora {

namespace {

// Один треугольник на весь экран: вершины строит сам шейдер по номеру вершины.
constexpr std::uint32_t kFullscreenTriangleVertices = 3;

constexpr rhi::BindingSlot kHdrTexture{kMaterialSet, 0};

// Определение, по которому output.glsl выбирает компрессию.
constexpr std::string_view kToneMappingDefinePrefix = "YQLORA_TONE_MAPPING_";

} // namespace

OutputPass::OutputPass(const RenderContext& context)
    : m_sampler(
          context.device.createSampler({.filter = rhi::Filter::Nearest, .wrap = rhi::Wrap::Clamp, .mipmaps = false})
      )
{
    for (const std::string_view name : EnumNames<ToneMapping>::names) {
        Variant variant;
        variant.shader = context.device.createShader(buildBuiltinShader(
            context.shaders,
            {
                .name = "output " + std::string(name),
                .vertexPath = engineShaderPath("fullscreen.vert"),
                .fragmentPath = engineShaderPath("output.frag"),
                .defines = {{.name = std::string(kToneMappingDefinePrefix) + std::string(name)}},
            }
        ));
        variant.pipeline = context.device.createPipeline({
            .shader = variant.shader.get(),
            .depthTest = false,
            .depthWrite = false,
            .cull = rhi::CullMode::None,
            .colorFormat = kOutputColorFormat,
            .depthFormat = rhi::Format::None,
        });
        m_variants.push_back(std::move(variant));
    }
}

OutputPass::~OutputPass() = default;

void OutputPass::addTo(
    RenderGraph& graph,
    RenderGraph::TextureId hdr,
    RenderGraph::TextureId target,
    ToneMapping toneMapping
) const
{
    const rhi::Pipeline& pipeline = *m_variants.at(static_cast<std::size_t>(toneMapping)).pipeline;
    graph.addPass({
        .name = "output",
        // Весь кадр перезаписывается, стирать незачем.
        .color = {target, rhi::LoadOp::Load},
        .reads = {hdr},
        .record = [this, hdr, &pipeline](rhi::CommandList& commands, const RenderGraph& passes) {
            commands.bindPipeline(pipeline);
            commands.setTexture(kHdrTexture.set, kHdrTexture.binding, passes.texture(hdr), *m_sampler);
            commands.draw(kFullscreenTriangleVertices);
        },
    });
}

} // namespace yqlora
