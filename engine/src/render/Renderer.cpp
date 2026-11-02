#include "yqlora/render/Renderer.hpp"

#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/render/GpuMesh.hpp"
#include "yqlora/render/GraphicsBackend.hpp"
#include "yqlora/render/MeshPass.hpp"
#include "yqlora/render/OutputPass.hpp"
#include "yqlora/render/Prepare.hpp"
#include "yqlora/render/RenderGraph.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/render/ViewPass.hpp"
#include "yqlora/shader/ShaderCompiler.hpp"

#include <span>
#include <stdexcept>
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

rhi::SamplerDesc samplerDesc(const TextureSampling& sampling)
{
    return {
        .filter = sampling.filter == TextureFilter::Nearest ? rhi::Filter::Nearest : rhi::Filter::Linear,
        .wrap = sampling.wrap == TextureWrap::Clamp    ? rhi::Wrap::Clamp
                : sampling.wrap == TextureWrap::Mirror ? rhi::Wrap::Mirror
                                                       : rhi::Wrap::Repeat,
        .mipmaps = sampling.mipmaps,
    };
}

} // namespace

Renderer::Renderer(GraphicsApi api, std::unique_ptr<rhi::Device> device, std::unique_ptr<UiIntegration> ui)
    : m_api(api)
    , m_device(std::move(device))
    , m_ui(std::move(ui))
    // Шейдеры движка вшиты, файлов проекта рендереру читать не нужно.
    , m_shaders(std::make_unique<ShaderCompiler>(ShaderFileReader{}))
    , m_uniforms(std::make_unique<UniformRing>(*m_device))
    , m_meshes(std::make_unique<GpuMeshCache>(*m_device))
    , m_uiTextures(std::make_unique<GpuResourceCache<rhi::Texture>>())
    , m_transients(std::make_unique<TransientTextures>(*m_device))
    , m_context(std::make_unique<RenderContext>(RenderContext{*m_device, *m_shaders, *m_uniforms, *m_meshes}))
    , m_meshPass(std::make_unique<MeshPass>(*m_context))
    , m_outputPass(std::make_unique<OutputPass>(*m_context))
{
}

Renderer::~Renderer()
{
    // Объекты устройства удаляются раньше него.
    m_outputPass.reset();
    m_meshPass.reset();
    m_transients.reset();
    m_uiTextures.reset();
    m_meshes.reset();
    m_uniforms.reset();
    m_shaders.reset();
    m_ui.reset();
    m_device.reset();
}

void Renderer::render(const FrameData& frame)
{
    // Задний буфер переходит в Present в present(): поверх ещё рисуется интерфейс.
    drawScene(frame, m_device->backBuffer(), m_device->backBufferDepth(), std::nullopt);
}

void Renderer::render(const FrameData& frame, RenderTarget& target)
{
    // Цель рендера дальше читает интерфейс.
    drawScene(frame, target.color(), target.depth(), rhi::ResourceState::ShaderRead);
}

std::unique_ptr<RenderTarget> Renderer::createRenderTarget(int width, int height)
{
    return std::make_unique<RenderTarget>(*m_device, m_ui.get(), width, height);
}

void Renderer::clearWindow(const Vec4& color)
{
    rhi::CommandList& commands = m_device->commands();
    commands.transition(m_device->backBuffer(), rhi::ResourceState::ColorTarget);
    commands.beginRenderPass(
        {.color = &m_device->backBuffer(), .depth = &m_device->backBufferDepth(), .clearColor = color}
    );
    commands.endRenderPass();
}

std::unique_ptr<UiRenderer> Renderer::createUiRenderer()
{
    return ui().createUiRenderer();
}

UiIntegration& Renderer::ui() const
{
    if (!m_ui) {
        throw std::logic_error("Renderer: created without an interface");
    }

    return *m_ui;
}

void Renderer::drawScene(
    const FrameData& frame,
    rhi::Texture& color,
    rhi::Texture& depth,
    std::optional<rhi::ResourceState> finalColorState
)
{
    RenderGraph graph(*m_transients);
    const RenderGraph::TextureId colorId = graph.import(color, finalColorState);
    const RenderGraph::TextureId depthId = graph.import(depth);
    if (frame.views.empty()) {
        graph.addPass({
            .name = "no camera",
            .color = {colorId, rhi::LoadOp::Clear},
            .depth = RenderGraph::Target{depthId, rhi::LoadOp::Clear},
            .clearColor = kNoViewColor,
        });
        graph.execute(m_device->commands());

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
    const UniformSlice frameUniforms = m_uniforms->push(yqlora::frameUniforms(frame));
    // Списки видимого живут до execute: на них ссылаются записанные проходы.
    std::vector<std::vector<const DrawItem*>> visible(frame.views.size());
    std::vector<UniformSlice> viewUniforms(frame.views.size());
    // Виды по очереди: следующая камера рисует поверх предыдущей.
    for (std::size_t i = 0; i < frame.views.size(); ++i) {
        const ViewData& view = frame.views[i];
        visible[i] = prepareView(frame.drawItems, view);
        viewUniforms[i] = m_uniforms->push(
            ViewUniforms{
                .viewProjection = view.camera.projection * view.camera.view,
                .cameraPosition = view.camera.position,
            }
        );
        graph.addPass({
            .name = "view " + std::to_string(i),
            .color = {hdrId, colorLoad(view.clear)},
            .depth = RenderGraph::Target{depthId, depthLoad(view.clear)},
            .clearColor = view.background,
            .record = [this,
                       items = std::span<const DrawItem* const>(visible[i]),
                       frameUniforms,
                       slice = viewUniforms[i]](rhi::CommandList& commands, const RenderGraph&) {
                m_meshPass->draw(commands, items, frameUniforms, slice);
            },
        });
    }
    m_outputPass->addTo(graph, hdrId, colorId);
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
    graph.execute(m_device->commands());
}

void Renderer::present()
{
    m_device->commands().transition(m_device->backBuffer(), rhi::ResourceState::Present);
    m_device->present();
    m_uniforms->reset();
    m_meshes->endFrame();
    m_uiTextures->endFrame();
    m_transients->endFrame();
}

UiTexture Renderer::uiTexture(ResourceId id, std::uint64_t revision, const TextureData& data)
{
    rhi::Texture& texture = m_uiTextures->get(id, revision, [&] {
        // Цвета как в файле, без перевода из sRGB: ImGui рисует в обычный буфер.
        return m_device->createTexture(
            {.width = data.width,
             .height = data.height,
             .format = rhi::Format::Rgba8,
             .mipmaps = data.sampling.mipmaps},
            std::as_bytes(std::span(data.pixels))
        );
    });

    return ui().texture(texture, samplerDesc(data.sampling));
}

bool Renderer::setVsync(bool enabled)
{
    return m_device->setVsync(enabled);
}

void Renderer::resize(int width, int height)
{
    m_device->resizeBackBuffer(width, height);
}

std::unique_ptr<Renderer> createRenderer(GraphicsApi api, Window& window)
{
    std::unique_ptr<rhi::Device> device = createDevice(api, window);
    std::unique_ptr<UiIntegration> ui = createUiIntegration(api, window, *device);

    return std::make_unique<Renderer>(api, std::move(device), std::move(ui));
}

} // namespace yqlora
