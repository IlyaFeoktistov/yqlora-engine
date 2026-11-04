#include "yqlora/editor/render/BackgroundPass.hpp"

#include "yqlora/editor/render/EditorShaders.hpp"
#include "yqlora/render/FrameFormats.hpp"
#include "yqlora/shader/ShaderBuild.hpp"
#include "yqlora/shader/ShaderCompiler.hpp"
#include "yqlora/shader/ShaderProfile.hpp"

namespace yqlora::editor {

namespace {

constexpr std::uint32_t kFullscreenTriangleVertices = 3;

// Блок Background в editor/shaders/background.frag.
struct alignas(16) BackgroundUniforms {
    Vec4 color{0.0f};
};

const UniformLayout kBackgroundBlock{
    "Background",
    {kMaterialSet, 0},
    sizeof(BackgroundUniforms),
    {YQLORA_UNIFORM_MEMBER(BackgroundUniforms, color)},
};

const UniformLayout* const kBackgroundLayouts[] = {&kBackgroundBlock};

} // namespace

BackgroundPass::BackgroundPass(const RenderContext& context)
    : m_uniforms(context.uniforms)
{
    ShaderCompiler shaders(readEditorShader);
    m_shader = context.device.createShader(buildBuiltinShader(
        shaders,
        {
            .name = "background",
            .vertexPath = editorShaderPath("background.vert"),
            .fragmentPath = editorShaderPath("background.frag"),
            .layouts = kBackgroundLayouts,
        }
    ));
    m_pipeline = context.device.createPipeline({
        .shader = m_shader.get(),
        .depthTest = true,
        .depthWrite = false,
        .depthCompare = rhi::CompareOp::LessEqual,
        .cull = rhi::CullMode::None,
        .colorFormat = kOutputColorFormat,
        .depthFormat = kDepthFormat,
    });
}

void BackgroundPass::addTo(RenderGraph& graph, const ViewPassInput& input)
{
    const UniformSlice uniforms = m_uniforms.push(BackgroundUniforms{.color = m_color});
    graph.addPass({
        .name = "background",
        .color = {input.color, rhi::LoadOp::Load},
        .depth = RenderGraph::Target{input.depth, rhi::LoadOp::Load},
        .record = [this, uniforms](rhi::CommandList& commands, const RenderGraph&) {
            commands.bindPipeline(*m_pipeline);
            setUniforms(commands, kBackgroundBlock, uniforms);
            commands.draw(kFullscreenTriangleVertices);
        },
    });
}

} // namespace yqlora::editor
