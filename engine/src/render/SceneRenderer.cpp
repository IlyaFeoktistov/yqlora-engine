#include "yqlora/render/SceneRenderer.hpp"

#include "yqlora/render/AtmospherePass.hpp"
#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/render/FrameFormats.hpp"
#include "yqlora/render/MeshPass.hpp"
#include "yqlora/render/OutputPass.hpp"
#include "yqlora/render/Prepare.hpp"
#include "yqlora/render/RenderGraph.hpp"
#include "yqlora/render/SkyPass.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/render/ViewPass.hpp"

#include <span>
#include <string>
#include <vector>

namespace yqlora {

namespace {

// Кадр без единого вида (в сцене нет активной камеры).
const Vec4 kNoViewColor(0.0f, 0.0f, 0.0f, 1.0f);

// Sky стирает цвет в чёрный: небо закрасит то, что останется пустым.
rhi::LoadOp colorLoad(CameraClear clear)
{
    return clear == CameraClear::ColorAndDepth || clear == CameraClear::Sky ? rhi::LoadOp::Clear : rhi::LoadOp::Load;
}

const Vec4 kSkyClearColor(0.0f, 0.0f, 0.0f, 1.0f);

rhi::LoadOp depthLoad(CameraClear clear)
{
    return clear == CameraClear::Nothing ? rhi::LoadOp::Load : rhi::LoadOp::Clear;
}

} // namespace

SceneRenderer::SceneRenderer(const RenderContext& context, TransientTextures& transients)
    : m_context(context)
    , m_transients(transients)
    , m_atmosphere(std::make_unique<AtmospherePass>(context))
    , m_meshPass(std::make_unique<MeshPass>(context))
    , m_outputPass(std::make_unique<OutputPass>(context))
{
}

SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::draw(
    const FrameData& frame,
    rhi::Texture& color,
    rhi::Texture& depth,
    std::optional<rhi::ResourceState> finalColorState
)
{
    RenderGraph graph(m_transients);
    const RenderGraph::TextureId colorId = graph.import(color, finalColorState);
    const RenderGraph::TextureId depthId = graph.import(depth);
    if (frame.views.empty()) {
        graph.addPass({
            .name = "no camera",
            .color = {colorId, rhi::LoadOp::Clear},
            .depth = RenderGraph::Target{depthId, rhi::LoadOp::Clear},
            .clearColor = kNoViewColor,
        });
        graph.execute(m_context.device.commands());

        return;
    }

    // Свет и смешивание прозрачного считаются в линейном RGB, поэтому сцена
    // идёт в HDR-цель с глубиной итоговой цели.
    const RenderGraph::TextureId hdrId = graph.create({
        .width = color.desc().width,
        .height = color.desc().height,
        .format = kHdrColorFormat,
        .usage = rhi::kColorTarget | rhi::kSampled,
    });
    const AtmosphereUniforms atmosphere = atmosphereUniforms(frame.environment.atmosphere);
    const EngineResources frameResources{
        .frame = m_context.uniforms.push(frameUniforms(frame)),
        .atmosphere = m_context.uniforms.push(atmosphere),
        .lutSampler = &m_atmosphere->lutSampler(),
    };
    const AtmospherePass::FrameLuts frameLuts = m_atmosphere->addFrame(graph, atmosphere, frameResources);
    // Списки видимого живут до execute: на них ссылаются записанные проходы.
    std::vector<PreparedView> prepared(frame.views.size());
    std::vector<UniformSlice> viewUniforms(frame.views.size());
    // Виды по очереди: следующая камера рисует поверх предыдущей.
    for (std::size_t i = 0; i < frame.views.size(); ++i) {
        const ViewData& view = frame.views[i];
        prepared[i] = prepareView(frame.drawItems, frame.materials, view);
        const Mat4 viewProjection = view.camera.projection * view.camera.view;
        viewUniforms[i] = m_context.uniforms.push(
            ViewUniforms{
                .viewProjection = viewProjection,
                .inverseViewProjection = glm::inverse(viewProjection),
                .cameraPosition = view.camera.position,
                .exposure = view.exposure.factor,
            }
        );
        EngineResources viewResources = frameResources;
        viewResources.view = viewUniforms[i];
        const AtmospherePass::ViewLuts luts = m_atmosphere->addView(graph, atmosphere, viewResources, frameLuts);
        graph.addPass({
            .name = "view " + std::to_string(i),
            .color = {hdrId, colorLoad(view.clear)},
            .depth = RenderGraph::Target{depthId, depthLoad(view.clear)},
            .clearColor = view.clear == CameraClear::Sky ? kSkyClearColor : view.background,
            .reads = {frameLuts.transmittance, luts.skyView, luts.aerialPerspective},
            .record = [this, &frame, &view, &items = prepared[i], viewResources, frameLuts, luts](
                          rhi::CommandList& commands,
                          const RenderGraph& passes
                      ) {
                EngineResources resources = viewResources;
                resources.transmittance = &passes.texture(frameLuts.transmittance);
                resources.skyView = &passes.texture(luts.skyView);
                resources.aerialPerspective = &passes.texture(luts.aerialPerspective);
                const std::span<const DrawItem* const> all(items.items);
                m_meshPass->draw(commands, frame, all.first(items.opaqueCount), resources);
                if (view.clear == CameraClear::Sky) {
                    drawSky(commands, m_context, frame, resources);
                }
                m_meshPass->draw(commands, frame, all.subspan(items.opaqueCount), resources);
            },
        });
    }
    m_outputPass->addTo(graph, hdrId, colorId, frame.views.front().exposure.toneMapping);
    // Проходы владельцев видов рисуют поверх итогового кадра, в sRGB, и
    // проверяют глубину сцены. В кадре из нескольких видов глубина - уже
    // общая, поэтому проходы владельца ставят виду, который рисуется один
    // (окно сцены в редакторе).
    for (std::size_t i = 0; i < frame.views.size(); ++i) {
        const ViewData& view = frame.views[i];
        const ViewPassInput input{
            .frame = frame,
            .view = view,
            .visible = prepared[i].items,
            .viewUniforms = viewUniforms[i],
            .color = colorId,
            .depth = depthId,
        };
        for (ViewPass* pass : view.passes) {
            pass->addTo(graph, input);
        }
    }
    graph.execute(m_context.device.commands());
}

} // namespace yqlora
