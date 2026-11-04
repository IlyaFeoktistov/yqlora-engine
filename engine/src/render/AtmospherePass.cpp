#include "yqlora/render/AtmospherePass.hpp"

#include "yqlora/math/Half.hpp"
#include "yqlora/render/ViewPass.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"
#include "yqlora/shader/ShaderProfile.hpp"

#include <array>
#include <span>
#include <string>
#include <utility>

namespace yqlora {

namespace {

// Таблицы - линейная яркость и пропускание, могут быть больше 1.
constexpr rhi::Format kLutFormat = rhi::Format::Rgba16Float;
constexpr std::uint32_t kFullscreenTriangleVertices = 3;
// Своя таблица многократного рассеяния у шейдеров вида (march.glsl).
constexpr rhi::BindingSlot kMultipleScatteringSlot{kMaterialSet, 0};
constexpr char kAerialSlicesDefine[] = "YQLORA_AERIAL_SLICES";

} // namespace

AtmospherePass::AtmospherePass(const RenderContext& context)
    : m_context(context)
    , m_sampler(
          context.device.createSampler({.filter = rhi::Filter::Linear, .wrap = rhi::Wrap::Clamp, .mipmaps = false})
      )
    , m_transmittanceLut(makeLut("atmosphere/transmittance.frag"))
    , m_multipleScatteringLut(makeLut("atmosphere/multiple_scattering.frag"))
    , m_skyViewLut(makeLut("atmosphere/sky_view.frag"))
    , m_aerialPerspectiveLut(makeLut(
          "atmosphere/aerial_perspective.frag",
          {{.name = kAerialSlicesDefine, .value = std::to_string(kAerialSlices)}}
      ))
    , m_transmittance(context.device.createTexture({
          .width = kTransmittanceWidth,
          .height = kTransmittanceHeight,
          .format = kLutFormat,
          .usage = rhi::kColorTarget | rhi::kSampled,
      }))
    , m_multipleScattering(context.device.createTexture({
          .width = kMultipleScatteringSize,
          .height = kMultipleScatteringSize,
          .format = kLutFormat,
          .usage = rhi::kColorTarget | rhi::kSampled,
      }))
{
    // Пропускание 1, рассеяния нет: если шейдер всё же прочтёт пустую
    // таблицу, она ничего не изменит.
    const std::array<std::uint16_t, 4> clear{0, 0, 0, math::toHalf(1.0f)};
    m_empty = context.device.createTexture({.format = kLutFormat}, std::as_bytes(std::span(clear)));
}

AtmospherePass::~AtmospherePass() = default;

AtmospherePass::Lut AtmospherePass::makeLut(const char* file, std::vector<ShaderDefine> defines)
{
    Lut lut;
    lut.shader = m_context.device.createShader(buildBuiltinShader(
        m_context.shaders,
        {
            .name = file,
            .vertexPath = engineShaderPath("fullscreen.vert"),
            .fragmentPath = engineShaderPath(file),
            .defines = std::move(defines),
            .layouts = kEngineBlocks,
        }
    ));
    lut.pipeline = m_context.device.createPipeline({
        .shader = lut.shader.get(),
        .depthTest = false,
        .depthWrite = false,
        .cull = rhi::CullMode::None,
        .colorFormat = kLutFormat,
        .depthFormat = rhi::Format::None,
    });

    return lut;
}

void AtmospherePass::addLutPass(
    RenderGraph& graph,
    const char* name,
    const Lut& lut,
    RenderGraph::TextureId target,
    std::vector<RenderGraph::TextureId> reads,
    const EngineResources& resources,
    std::optional<RenderGraph::TextureId> transmittance,
    std::optional<RenderGraph::TextureId> multipleScattering
)
{
    graph.addPass({
        .name = name,
        // Таблица перезаписывается целиком.
        .color = {target, rhi::LoadOp::Load},
        .reads = std::move(reads),
        .record = [this, &lut, resources, transmittance, multipleScattering](
                      rhi::CommandList& commands,
                      const RenderGraph& passes
                  ) {
            EngineResources bound = resources;
            bound.lutSampler = m_sampler.get();
            if (transmittance) {
                bound.transmittance = &passes.texture(*transmittance);
            }
            commands.bindPipeline(*lut.pipeline);
            bindEngineResources(commands, *lut.shader, bound);
            if (multipleScattering) {
                commands.setTexture(
                    kMultipleScatteringSlot.set,
                    kMultipleScatteringSlot.binding,
                    passes.texture(*multipleScattering),
                    *m_sampler
                );
            }
            commands.draw(kFullscreenTriangleVertices);
        },
    });
}

AtmospherePass::FrameLuts AtmospherePass::addFrame(
    RenderGraph& graph,
    const AtmosphereUniforms& atmosphere,
    const EngineResources& resources
)
{
    const FrameLuts luts{
        .transmittance = graph.import(*m_transmittance),
        .multipleScattering = graph.import(*m_multipleScattering),
        .empty = graph.import(*m_empty),
    };
    if (atmosphere.enabled == 0 || m_filledFor == atmosphere) {
        return luts;
    }
    m_filledFor = atmosphere;
    addLutPass(graph, "atmosphere transmittance", m_transmittanceLut, luts.transmittance, {}, resources, {}, {});
    addLutPass(
        graph,
        "atmosphere multiple scattering",
        m_multipleScatteringLut,
        luts.multipleScattering,
        {luts.transmittance},
        resources,
        luts.transmittance,
        {}
    );

    return luts;
}

AtmospherePass::ViewLuts AtmospherePass::addView(
    RenderGraph& graph,
    const AtmosphereUniforms& atmosphere,
    const EngineResources& resources,
    const FrameLuts& frame
)
{
    if (atmosphere.enabled == 0) {
        return {.skyView = frame.empty, .aerialPerspective = frame.empty};
    }

    const RenderGraph::TextureId transmittance = frame.transmittance;
    const RenderGraph::TextureId multiple = frame.multipleScattering;
    const ViewLuts luts{
        .skyView = graph.create(
            {.width = kSkyViewWidth,
             .height = kSkyViewHeight,
             .format = kLutFormat,
             .usage = rhi::kColorTarget | rhi::kSampled}
        ),
        .aerialPerspective = graph.create({
            .width = kAerialSlices * kAerialSliceSize,
            .height = kAerialSliceSize,
            .format = kLutFormat,
            .usage = rhi::kColorTarget | rhi::kSampled,
        }),
    };
    addLutPass(
        graph,
        "atmosphere sky view",
        m_skyViewLut,
        luts.skyView,
        {transmittance, multiple},
        resources,
        transmittance,
        multiple
    );
    addLutPass(
        graph,
        "atmosphere aerial perspective",
        m_aerialPerspectiveLut,
        luts.aerialPerspective,
        {transmittance, multiple},
        resources,
        transmittance,
        multiple
    );

    return luts;
}

} // namespace yqlora
