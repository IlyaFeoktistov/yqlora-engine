#include "yqlora/opengl/GridPass.hpp"

#include "GeneratedShaders.hpp"
#include "yqlora/render/ShaderData.hpp"

#include <array>
#include <cstddef>
#include <span>

namespace yqlora::opengl {

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

// Блок Grid в шейдерах сетки.
struct alignas(16) GridUniforms {
    float cellSize = kCellSize;
    float fadeDistance = kFadeDistance;
};

const rhi::ShaderBinding kGridBlock{"Grid", rhi::BindingKind::UniformBlock, kMaterialSet, 0};

} // namespace

GridPass::GridPass(rhi::Device& device, UniformRing& uniforms)
    : m_uniforms(uniforms)
    , m_corners(device.createBuffer(
          {.size = sizeof(kCorners), .usage = rhi::BufferUsage::Vertex},
          std::as_bytes(std::span(kCorners))
      ))
    , m_shader(device.createShader({
          .name = "grid",
          .vertex = shaders::kGridVert,
          .fragment = shaders::kGridFrag,
          .bindings = {kViewBlock, kGridBlock},
      }))
    , m_pipeline(device.createPipeline({
          .shader = m_shader.get(),
          .vertexLayout = {.stride = sizeof(Vec2), .attributes = {{0, rhi::VertexFormat::Float2, 0}}},
          .depthTest = true,
          .depthWrite = false,
          // Снизу сетку тоже видно.
          .cull = rhi::CullMode::None,
          .blend = rhi::BlendMode::Alpha,
      }))
{
}

void GridPass::draw(rhi::CommandList& commands, const UniformSlice& viewUniforms) const
{
    commands.bindPipeline(*m_pipeline);
    setUniforms(commands, kViewBlock, viewUniforms);
    setUniforms(commands, kGridBlock, m_uniforms.push(GridUniforms{}));
    commands.setVertexBuffer(*m_corners);
    commands.draw(kCornerCount);
}

} // namespace yqlora::opengl
