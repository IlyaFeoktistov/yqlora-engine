#include "yqlora/render/ShadowPass.hpp"

#include "yqlora/material/MaterialData.hpp"
#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/render/GpuMesh.hpp"
#include "yqlora/render/MaterialBinding.hpp"
#include "yqlora/render/PipelineCache.hpp"
#include "yqlora/render/Prepare.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/render/ViewPass.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"

namespace yqlora {

namespace {

rhi::Viewport cellOf(int index, int columns, int cellSize)
{
    return {
        .x = (index % columns) * cellSize,
        .y = (index / columns) * cellSize,
        .width = cellSize,
        .height = cellSize,
    };
}

// Материал меша с альфа-тестом: порог (из замены, если она есть) больше 0 и
// у шейдера есть вариант глубины.
bool alphaTested(const FrameMaterial& material, const FrameOverrides* overrides)
{
    const MaterialField* field = material.shader ? material.shader->findField(kAlphaCutoffField) : nullptr;
    if (!field || !material.shader->depthDesc || !material.data) {
        return false;
    }
    if (overrides) {
        const auto& values = overrides->values.values();
        if (const auto found = values.find(kAlphaCutoffField);
            found != values.end() && !found->second.numbers.empty()) {
            return found->second.numbers.front() > 0.0f;
        }
    }
    const std::span<const float> value = materialValue(*material.data, *field);

    return !value.empty() && value.front() > 0.0f;
}

} // namespace

ShadowPass::ShadowPass(const RenderContext& context)
    : m_context(context)
    , m_shader(context.device.createShader(buildBuiltinShader(
          context.shaders,
          {
              .name = "shadow depth",
              .vertexPath = engineShaderPath("shadow/depth.vert"),
              .fragmentPath = engineShaderPath("shadow/depth.frag"),
              .layouts = kEngineBlocks,
          }
      )))
    , m_pipeline(context.device.createPipeline({
          .shader = m_shader.get(),
          .vertexLayout = meshVertexLayout(),
          .depthBias = kDepthBias,
          // Тень отбрасывают и тонкие поверхности, видимые с одной стороны.
          .cull = rhi::CullMode::None,
          .colorFormat = rhi::Format::None,
          .depthFormat = kFormat,
      }))
    , m_compareSampler(context.device.createSampler(
          {.filter = rhi::Filter::Linear, .wrap = rhi::Wrap::Clamp, .mipmaps = false, .compare = true}
      ))
    , m_depthSampler(
          context.device.createSampler({.filter = rhi::Filter::Nearest, .wrap = rhi::Wrap::Clamp, .mipmaps = false})
      )
{
}

ShadowPass::~ShadowPass() = default;

RenderGraph::TextureId ShadowPass::addAtlas(RenderGraph& graph, const FrameData& frame)
{
    const int tileSize = frame.shadows.atlasSize / kShadowAtlasColumns;
    std::vector<Region> regions;
    for (std::size_t i = 0; i < frame.shadowTiles.size(); ++i) {
        regions.push_back(
            {frame.shadowTiles[i].viewProjection, cellOf(static_cast<int>(i), kShadowAtlasColumns, tileSize)}
        );
    }

    return addMap(graph, frame, "shadow atlas", frame.shadows.atlasSize, std::move(regions));
}

RenderGraph::TextureId ShadowPass::addCascades(
    RenderGraph& graph,
    const FrameData& frame,
    const ViewData& view,
    std::vector<ShadowCascade>& cascades
)
{
    cascades.clear();
    const auto sun = std::ranges::find_if(frame.lights, [](const LightData& light) {
        return light.sun && light.shadows != ShadowMode::None;
    });
    if (frame.shadows.enabled && sun != frame.lights.end()) {
        cascades = fitShadowCascades(view.camera, -sun->direction, frame.shadows, frame.drawItems);
    }
    // Один каскад - вся карта, больше - 2 x 2 (shadows.glsl, cascadeRect).
    const int columns = cascades.size() > 1 ? kShadowCascadeColumns : 1;
    std::vector<Region> regions;
    for (std::size_t i = 0; i < cascades.size(); ++i) {
        regions.push_back(
            {cascades[i].viewProjection, cellOf(static_cast<int>(i), columns, frame.shadows.cascadeSize)}
        );
    }

    return addMap(graph, frame, "shadow cascades", frame.shadows.cascadeSize * columns, std::move(regions));
}

RenderGraph::TextureId ShadowPass::addMap(
    RenderGraph& graph,
    const FrameData& frame,
    const char* name,
    int size,
    std::vector<Region> regions
)
{
    const int side = regions.empty() ? 1 : size;
    const RenderGraph::TextureId map = graph.create({
        .width = side,
        .height = side,
        .format = kFormat,
        .usage = rhi::kDepthTarget | rhi::kSampled,
    });
    // Кто в какую карту попадает - сейчас, в record остаётся только рисовать.
    // Сначала сплошные (один конвейер на всех), за ними с альфа-тестом.
    std::vector<std::vector<Caster>> casters(regions.size());
    for (std::size_t i = 0; i < regions.size(); ++i) {
        const Frustum frustum = frustumFromMatrix(regions[i].viewProjection);
        std::vector<Caster> tested;
        for (const DrawItem& item : frame.drawItems) {
            const FrameMaterial& material = frame.materials[item.material];
            const bool opaque = material.data->blend != MaterialBlend::Transparent;
            if (!item.castShadows || !opaque || !intersects(frustum, item.bounds)) {
                continue;
            }
            const FrameOverrides* overrides = item.overrides != DrawItem::kNoOverrides
                                                  ? &frame.overrides[item.overrides]
                                                  : nullptr;
            if (alphaTested(material, overrides)) {
                tested.push_back({&item, &material, overrides});
            } else {
                casters[i].push_back({&item});
            }
        }
        casters[i].insert(casters[i].end(), tested.begin(), tested.end());
    }
    graph.addPass({
        .name = name,
        .depth = RenderGraph::Target{map, rhi::LoadOp::Clear},
        .record = [this,
                   regions = std::move(regions),
                   casters = std::move(casters)](rhi::CommandList& commands, const RenderGraph&) {
            for (std::size_t i = 0; i < regions.size(); ++i) {
                if (casters[i].empty()) {
                    continue;
                }
                commands.setViewport(regions[i].viewport);
                const UniformSlice view = m_context.uniforms.push(
                    ViewUniforms{.viewProjection = regions[i].viewProjection}
                );
                m_bound = nullptr;
                for (const Caster& caster : casters[i]) {
                    drawCaster(commands, caster, view);
                }
            }
        },
    });

    return map;
}

void ShadowPass::drawCaster(rhi::CommandList& commands, const Caster& caster, const UniformSlice& view)
{
    const rhi::Pipeline* pipeline = caster.material ? m_context.pipelines.get(
                                                          *caster.material,
                                                          MaterialPass::ShadowDepth,
                                                          rhi::Format::None,
                                                          kFormat
                                                      )
                                                    : nullptr;
    // Вариант глубины бэкенд не собрал - тень сплошная, как у остальных.
    if (!pipeline) {
        pipeline = m_pipeline.get();
    }
    if (pipeline != m_bound) {
        commands.bindPipeline(*pipeline);
        setUniforms(commands, kViewBlock, view);
        m_bound = pipeline;
    }
    if (pipeline != m_pipeline.get()) {
        bindMaterial(commands, m_context, *pipeline->desc().shader, *caster.material, caster.overrides);
    }
    const DrawItem& item = *caster.item;
    setUniforms(commands, kObjectBlock, m_context.uniforms.push(ObjectUniforms{.model = item.world}));
    drawMesh(commands, m_context.meshes.get(item), item.firstIndex, item.indexCount);
}

} // namespace yqlora
