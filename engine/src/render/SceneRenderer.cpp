#include "yqlora/render/SceneRenderer.hpp"

#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/render/FrameFormats.hpp"
#include "yqlora/render/MeshPass.hpp"
#include "yqlora/render/OutputPass.hpp"
#include "yqlora/render/Prepare.hpp"
#include "yqlora/render/RenderGraph.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/render/ViewPass.hpp"

#include <span>
#include <string>
#include <vector>

namespace yqlora {

namespace {

// Кадр без единого вида (в сцене нет активной камеры).
const Vec4 kNoViewColor(0.0f, 0.0f, 0.0f, 1.0f);

rhi::LoadOp colorLoad(CameraClear clear)
{
    return clear == CameraClear::ColorAndDepth ? rhi::LoadOp::Clear : rhi::LoadOp::Load;
}

rhi::LoadOp depthLoad(CameraClear clear)
{
    return clear == CameraClear::Nothing ? rhi::LoadOp::Load : rhi::LoadOp::Clear;
}

} // namespace

SceneRenderer::SceneRenderer(const RenderContext& context, TransientTextures& transients)
    : m_context(context)
    , m_transients(transients)
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
    const UniformSlice frameUniforms = m_context.uniforms.push(yqlora::frameUniforms(frame));
    // Списки видимого живут до execute: на них ссылаются записанные проходы.
    std::vector<std::vector<const DrawItem*>> visible(frame.views.size());
    std::vector<UniformSlice> viewUniforms(frame.views.size());
    // Виды по очереди: следующая камера рисует поверх предыдущей.
    for (std::size_t i = 0; i < frame.views.size(); ++i) {
        const ViewData& view = frame.views[i];
        visible[i] = prepareView(frame.drawItems, frame.materials, view);
        const Mat4 viewProjection = view.camera.projection * view.camera.view;
        viewUniforms[i] = m_context.uniforms.push(
            ViewUniforms{
                .viewProjection = viewProjection,
                .inverseViewProjection = glm::inverse(viewProjection),
                .cameraPosition = view.camera.position,
                .exposure = view.exposure.factor,
            }
        );
        graph.addPass({
            .name = "view " + std::to_string(i),
            .color = {hdrId, colorLoad(view.clear)},
            .depth = RenderGraph::Target{depthId, depthLoad(view.clear)},
            .clearColor = view.background,
            .record = [this,
                       &frame,
                       items = std::span<const DrawItem* const>(visible[i]),
                       frameUniforms,
                       slice = viewUniforms[i]](rhi::CommandList& commands, const RenderGraph&) {
                m_meshPass->draw(commands, frame, items, frameUniforms, slice);
            },
        });
    }
    // Все виды кадра в одной HDR-цели, поэтому компрессия одна: главной камеры.
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
            .visible = visible[i],
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
