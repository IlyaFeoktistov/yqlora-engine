#include "yqlora/editor/render/OutlinePass.hpp"

#include "yqlora/editor/render/EditorShaders.hpp"

#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/render/FrameFormats.hpp"
#include "yqlora/render/GpuMesh.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"
#include "yqlora/shader/ShaderCompiler.hpp"

#include <algorithm>

namespace yqlora::editor {

namespace {

// Один треугольник на весь экран: вершины строит сам шейдер по номеру вершины.
constexpr std::uint32_t kFullscreenTriangleVertices = 3;

const Vec4 kMaskClear(0.0f);
// Маска выделенного: один канал, 1 там, где выделенный объект.
constexpr rhi::Format kMaskFormat = rhi::Format::R8;

// Блок Outline в editor/shaders/outline.frag.
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

OutlinePass::OutlinePass(const RenderContext& context)
    : m_meshes(context.meshes)
    , m_uniforms(context.uniforms)
    , m_maskSampler(
          context.device.createSampler({.filter = rhi::Filter::Nearest, .wrap = rhi::Wrap::Clamp, .mipmaps = false})
      )
{
    ShaderCompiler shaders(readEditorShader);
    m_maskShader = context.device.createShader(buildBuiltinShader(
        shaders,
        {
            .name = "outline mask",
            .vertexPath = editorShaderPath("outline_mask.vert"),
            .fragmentPath = editorShaderPath("outline_mask.frag"),
            .layouts = kEngineBlocks,
        }
    ));
    m_outlineShader = context.device.createShader(buildBuiltinShader(
        shaders,
        {
            .name = "outline",
            .vertexPath = engineShaderPath("fullscreen.vert"),
            .fragmentPath = editorShaderPath("outline.frag"),
            .layouts = kOutlineLayouts,
        }
    ));
    // Маска без теста глубины: контур виден сквозь другие объекты.
    m_maskPipeline = context.device.createPipeline({
        .shader = m_maskShader.get(),
        .vertexLayout = meshVertexLayout(),
        .depthTest = false,
        .depthWrite = false,
        .cull = rhi::CullMode::None,
        .colorFormat = kMaskFormat,
        .depthFormat = rhi::Format::None,
    });
    m_outlinePipeline = context.device.createPipeline({
        .shader = m_outlineShader.get(),
        .depthTest = false,
        .depthWrite = false,
        .cull = rhi::CullMode::None,
        .colorFormat = kOutputColorFormat,
        .depthFormat = kDepthFormat,
    });
}

void OutlinePass::addTo(RenderGraph& graph, const ViewPassInput& input)
{
    // Контур только у видимого: за пределами вида обводить нечего.
    std::vector<const DrawItem*> selected;
    for (const DrawItem* item : input.visible) {
        if (std::ranges::find(m_selected, item->node) != m_selected.end()) {
            selected.push_back(item);
        }
    }
    if (selected.empty()) {
        return;
    }

    const rhi::TextureDesc& target = graph.desc(input.color);
    const RenderGraph::TextureId mask = graph.create({
        .width = target.width,
        .height = target.height,
        .format = kMaskFormat,
        .usage = rhi::kColorTarget | rhi::kSampled,
    });
    const UniformSlice viewUniforms = input.viewUniforms;
    std::vector<UniformSlice> objects;
    objects.reserve(selected.size());
    for (const DrawItem* item : selected) {
        objects.push_back(m_uniforms.push(ObjectUniforms{.model = item->world}));
    }
    graph.addPass({
        .name = "outline mask",
        .color = {mask, rhi::LoadOp::Clear},
        .clearColor = kMaskClear,
        .record = [this, selected, objects, viewUniforms](rhi::CommandList& commands, const RenderGraph&) {
            commands.bindPipeline(*m_maskPipeline);
            setUniforms(commands, kViewBlock, viewUniforms);
            for (std::size_t i = 0; i < selected.size(); ++i) {
                setUniforms(commands, kObjectBlock, objects[i]);
                drawMesh(commands, m_meshes.get(*selected[i]), selected[i]->firstIndex, selected[i]->indexCount);
            }
        },
    });

    const UniformSlice outline = m_uniforms.push(OutlineUniforms{m_color});
    graph.addPass({
        .name = "outline",
        .color = {input.color, rhi::LoadOp::Load},
        .depth = RenderGraph::Target{input.depth, rhi::LoadOp::Load},
        .reads = {mask},
        .record = [this, mask, outline](rhi::CommandList& commands, const RenderGraph& passes) {
            commands.bindPipeline(*m_outlinePipeline);
            setUniforms(commands, kOutlineBlock, outline);
            commands.setTexture(kMaskTexture.set, kMaskTexture.binding, passes.texture(mask), *m_maskSampler);
            commands.draw(kFullscreenTriangleVertices);
        },
    });
}

} // namespace yqlora::editor
