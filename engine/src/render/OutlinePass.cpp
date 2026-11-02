#include "yqlora/render/OutlinePass.hpp"

#include "yqlora/render/GpuMesh.hpp"
#include "yqlora/render/ShaderData.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"

#include <algorithm>

namespace yqlora {

namespace {

// Один треугольник на весь экран: вершины строит сам шейдер по gl_VertexID.
constexpr std::uint32_t kFullscreenTriangleVertices = 3;

const Vec4 kMaskClear(0.0f);

// Блок Outline в engine/shaders/outline.frag.
struct alignas(16) OutlineUniforms {
    Vec4 color{1.0f};
};

const UniformLayout kOutlineBlock{
    "Outline",
    {kMaterialSet, 0},
    sizeof(OutlineUniforms),
    {YQLORA_UNIFORM_MEMBER(OutlineUniforms, color)},
};

const UniformLayout* const kOutlineLayouts[] = {&kOutlineBlock};

constexpr rhi::BindingSlot kMaskTexture{kMaterialSet, 1};

} // namespace

OutlinePass::OutlinePass(rhi::Device& device, ShaderCompiler& shaders, GpuMeshCache& meshes, UniformRing& uniforms)
    : m_device(device)
    , m_meshes(meshes)
    , m_uniforms(uniforms)
    , m_maskSampler(device.createSampler({.filter = rhi::Filter::Nearest, .wrap = rhi::Wrap::Clamp, .mipmaps = false}))
    , m_maskShader(device.createShader(buildEngineShader(
          shaders,
          {
              .name = "outline mask",
              .vertexPath = engineShaderPath("outline_mask.vert"),
              .fragmentPath = engineShaderPath("outline_mask.frag"),
              .layouts = kEngineBlocks,
          }
      )))
    , m_outlineShader(device.createShader(buildEngineShader(
          shaders,
          {
              .name = "outline",
              .vertexPath = engineShaderPath("fullscreen.vert"),
              .fragmentPath = engineShaderPath("outline.frag"),
              .layouts = kOutlineLayouts,
          }
      )))
    // Маска без теста глубины: контур виден сквозь другие объекты.
    , m_maskPipeline(device.createPipeline({
          .shader = m_maskShader.get(),
          .vertexLayout = meshVertexLayout(),
          .depthTest = false,
          .depthWrite = false,
          .cull = rhi::CullMode::None,
          .colorFormat = rhi::Format::R8,
          .depthFormat = rhi::Format::None,
      }))
    , m_outlinePipeline(device.createPipeline({
          .shader = m_outlineShader.get(),
          .depthTest = false,
          .depthWrite = false,
          .cull = rhi::CullMode::None,
      }))
{
}

void OutlinePass::draw(
    rhi::CommandList& commands,
    const FrameData& frame,
    const UniformSlice& viewUniforms,
    rhi::Texture& color,
    rhi::Texture* depth
)
{
    const auto isSelected = [&](const DrawItem& item) {
        return std::ranges::find(frame.editor.selected, item.node) != frame.editor.selected.end();
    };
    if (!std::ranges::any_of(frame.drawItems, isSelected)) {
        return;
    }
    resizeMask(color.desc().width, color.desc().height);

    // 1. Маска выбранных узлов.
    commands.transition(*m_mask, rhi::ResourceState::ColorTarget);
    commands.beginRenderPass({.color = m_mask.get(), .clearColor = kMaskClear});
    commands.bindPipeline(*m_maskPipeline);
    setUniforms(commands, kViewBlock, viewUniforms);
    for (const DrawItem& item : frame.drawItems) {
        if (!isSelected(item)) {
            continue;
        }
        setUniforms(commands, kObjectBlock, m_uniforms.push(ObjectUniforms{.model = item.world}));
        drawMesh(commands, m_meshes.get(item));
    }
    commands.endRenderPass();

    // 2. Контур поверх кадра.
    commands.transition(*m_mask, rhi::ResourceState::ShaderRead);
    commands.beginRenderPass({
        .color = &color,
        .depth = depth,
        .colorLoad = rhi::LoadOp::Load,
        .depthLoad = rhi::LoadOp::Load,
    });
    commands.bindPipeline(*m_outlinePipeline);
    setUniforms(commands, kOutlineBlock, m_uniforms.push(OutlineUniforms{frame.editor.outlineColor}));
    commands.setTexture(kMaskTexture.set, kMaskTexture.binding, *m_mask, *m_maskSampler);
    commands.draw(kFullscreenTriangleVertices);
    commands.endRenderPass();
}

void OutlinePass::resizeMask(int width, int height)
{
    if (m_mask && m_mask->desc().width == width && m_mask->desc().height == height) {
        return;
    }
    m_mask = m_device.createTexture({
        .width = width,
        .height = height,
        .format = rhi::Format::R8,
        .usage = rhi::kColorTarget | rhi::kSampled,
    });
}

} // namespace yqlora
