#include "yqlora/editor/render/GridPass.hpp"

#include "yqlora/editor/render/EditorShaders.hpp"

#include "yqlora/render/ShaderData.hpp"
#include "yqlora/shader/ShaderBuild.hpp"
#include "yqlora/shader/ShaderCompiler.hpp"

#include <array>
#include <cstddef>
#include <span>

namespace yqlora::editor {

namespace {

// Квадрат [-1, 1] из двух треугольников, размер и положение задаёт вершинный шейдер.
// clang-format off
constexpr std::array kCorners{
    -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
    -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f,
};
// clang-format on
constexpr std::uint32_t kCornerCount = kCorners.size() / Vec2::length();

// Шаг мелкой сетки в метрах; каждая десятая линия крупная.
constexpr float kCellSize = 1.0f;

// Дальше этого расстояния от камеры сетки не видно. Это же половина стороны квадрата.
constexpr float kFadeDistance = 150.0f;

// Блок Grid в шейдерах сетки (editor/shaders/grid.glsl).
struct alignas(16) GridUniforms {
    float cellSize = kCellSize;
    float fadeDistance = kFadeDistance;
};

const UniformLayout kGridBlock{
    "Grid",
    {kMaterialSet, 0},
    sizeof(GridUniforms),
    {
        YQLORA_UNIFORM_MEMBER(GridUniforms, cellSize),
        YQLORA_UNIFORM_MEMBER(GridUniforms, fadeDistance),
    },
};

const UniformLayout* const kGridLayouts[] = {&kViewBlock, &kGridBlock};

} // namespace

GridPass::GridPass(const RenderContext& context)
    : m_uniforms(context.uniforms)
    , m_corners(context.device.createBuffer(
          {.size = sizeof(kCorners), .usage = rhi::BufferUsage::Vertex},
          std::as_bytes(std::span(kCorners))
      ))
{
    ShaderCompiler shaders(readEditorShader);
    m_shader = context.device.createShader(buildBuiltinShader(
        shaders,
        {
            .name = "grid",
            .vertexPath = editorShaderPath("grid.vert"),
            .fragmentPath = editorShaderPath("grid.frag"),
            .layouts = kGridLayouts,
        }
    ));
    m_pipeline = context.device.createPipeline({
        .shader = m_shader.get(),
        .vertexLayout = {.stride = sizeof(Vec2), .attributes = {{0, rhi::VertexFormat::Float2, 0}}},
        .depthTest = true,
        .depthWrite = false,
        // Снизу сетку тоже видно.
        .cull = rhi::CullMode::None,
        .blend = rhi::BlendMode::Alpha,
    });
}

void GridPass::addTo(RenderGraph& graph, const ViewPassInput& input)
{
    const UniformSlice viewUniforms = input.viewUniforms;
    const UniformSlice gridUniforms = m_uniforms.push(GridUniforms{});
    graph.addPass({
        .name = "grid",
        .color = {input.color, rhi::LoadOp::Load},
        .depth = RenderGraph::Target{input.depth, rhi::LoadOp::Load},
        .record = [this, viewUniforms, gridUniforms](rhi::CommandList& commands, const RenderGraph&) {
            commands.bindPipeline(*m_pipeline);
            setUniforms(commands, kViewBlock, viewUniforms);
            setUniforms(commands, kGridBlock, gridUniforms);
            commands.setVertexBuffer(*m_corners);
            commands.draw(kCornerCount);
        },
    });
}

} // namespace yqlora::editor
