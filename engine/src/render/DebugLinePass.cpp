#include "yqlora/render/DebugLinePass.hpp"

#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/render/FrameFormats.hpp"
#include "yqlora/render/ViewPass.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace yqlora {

namespace {

const UniformLayout* const kLineLayouts[] = {&kViewBlock};

rhi::VertexLayout lineVertexLayout()
{
    return {
        .stride = sizeof(DebugVertex),
        .attributes = {
            {0, rhi::VertexFormat::Float3, offsetof(DebugVertex, position)},
            {1, rhi::VertexFormat::Float4, offsetof(DebugVertex, color)},
        },
    };
}

std::uint32_t vertexCount(const std::vector<DebugVertex>& vertices)
{
    return static_cast<std::uint32_t>(vertices.size());
}

} // namespace

DebugLinePass::DebugLinePass(const RenderContext& context)
    : m_vertices(context.vertices)
{
    m_shader = context.device.createShader(buildBuiltinShader(
        context.shaders,
        {
            .name = "debug lines",
            .vertexPath = engineShaderPath("debug/line.vert"),
            .fragmentPath = engineShaderPath("debug/line.frag"),
            .layouts = kLineLayouts,
        }
    ));
    const rhi::PipelineDesc tested{
        .shader = m_shader.get(),
        .vertexLayout = lineVertexLayout(),
        .depthTest = true,
        .depthWrite = false,
        .depthCompare = rhi::CompareOp::LessEqual,
        .cull = rhi::CullMode::None,
        .blend = rhi::BlendMode::Alpha,
        .topology = rhi::Topology::Lines,
        .colorFormat = kHdrColorFormat,
        .depthFormat = kDepthFormat,
    };
    m_tested = context.device.createPipeline(tested);
    rhi::PipelineDesc overlay = tested;
    overlay.depthTest = false;
    m_overlay = context.device.createPipeline(overlay);
}

DebugLinePass::~DebugLinePass() = default;

DebugLinePass::Lines DebugLinePass::upload(const FrameData& frame) const
{
    const DebugLineData& data = frame.debugLines;
    Lines lines{.testedCount = vertexCount(data.tested), .overlayCount = vertexCount(data.overlay)};
    if (lines.testedCount > 0) {
        lines.tested = m_vertices.push(std::as_bytes(std::span(data.tested)));
    }
    if (lines.overlayCount > 0) {
        lines.overlay = m_vertices.push(std::as_bytes(std::span(data.overlay)));
    }

    return lines;
}

void DebugLinePass::draw(rhi::CommandList& commands, const Lines& lines, const UniformSlice& view) const
{
    const auto drawWith = [&](const rhi::Pipeline& pipeline, const BufferSlice& vertices, std::uint32_t count) {
        if (count == 0) {
            return;
        }
        commands.bindPipeline(pipeline);
        setUniforms(commands, kViewBlock, view);
        commands.setVertexBuffer(*vertices.buffer, vertices.offset);
        commands.draw(count);
    };
    drawWith(*m_tested, lines.tested, lines.testedCount);
    drawWith(*m_overlay, lines.overlay, lines.overlayCount);
}

} // namespace yqlora
