#include "yqlora/render/Renderer.hpp"

#include "yqlora/render/GpuMesh.hpp"
#include "yqlora/render/GraphicsBackend.hpp"
#include "yqlora/render/GridPass.hpp"
#include "yqlora/render/MeshPass.hpp"
#include "yqlora/render/OutlinePass.hpp"
#include "yqlora/render/ShaderData.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/shader/ShaderCompiler.hpp"

#include <span>
#include <stdexcept>

namespace yqlora {

namespace {

// Кадр без единого вида (в сцене нет активной камеры).
const Vec4 kNoViewColor(0.0f, 0.0f, 0.0f, 1.0f);

// Свет падает сверху, немного сбоку и спереди.
const Vec3 kLightDirection = glm::normalize(Vec3(-0.4f, -1.0f, -0.3f));

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
    , m_meshPass(std::make_unique<MeshPass>(*m_device, *m_shaders, *m_meshes, *m_uniforms))
    , m_grid(std::make_unique<GridPass>(*m_device, *m_shaders, *m_uniforms))
    , m_outline(std::make_unique<OutlinePass>(*m_device, *m_shaders, *m_meshes, *m_uniforms))
{
}

Renderer::~Renderer()
{
    // Объекты устройства удаляются раньше него.
    m_outline.reset();
    m_grid.reset();
    m_meshPass.reset();
    m_uiTextures.reset();
    m_meshes.reset();
    m_uniforms.reset();
    m_shaders.reset();
    m_ui.reset();
    m_device.reset();
}

void Renderer::render(const FrameData& frame)
{
    drawScene(frame, m_device->backBuffer(), m_device->backBufferDepth());
}

void Renderer::render(const FrameData& frame, RenderTarget& target)
{
    drawScene(frame, target.color(), target.depth());
    m_device->commands().transition(target.color(), rhi::ResourceState::ShaderRead);
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

void Renderer::drawScene(const FrameData& frame, rhi::Texture& color, rhi::Texture& depth)
{
    rhi::CommandList& commands = m_device->commands();
    commands.transition(color, rhi::ResourceState::ColorTarget);
    commands.transition(depth, rhi::ResourceState::DepthTarget);
    if (frame.views.empty()) {
        commands.beginRenderPass({.color = &color, .depth = &depth, .clearColor = kNoViewColor});
        commands.endRenderPass();

        return;
    }

    const UniformSlice frameUniforms = m_uniforms->push(FrameUniforms{.lightDirection = kLightDirection});
    UniformSlice viewUniforms;
    // Виды по очереди: следующая камера рисует поверх предыдущей.
    for (std::size_t i = 0; i < frame.views.size(); ++i) {
        const ViewData& view = frame.views[i];
        viewUniforms = m_uniforms->push(
            ViewUniforms{
                .viewProjection = view.camera.projection * view.camera.view,
                .cameraPosition = view.camera.position,
            }
        );
        commands.beginRenderPass({
            .color = &color,
            .depth = &depth,
            .colorLoad = colorLoad(view.clear),
            .depthLoad = depthLoad(view.clear),
            .clearColor = view.background,
        });
        m_meshPass->draw(commands, frame, view, frameUniforms, viewUniforms);
        // Сетка полупрозрачная, поэтому рисуется после объектов сцены, в последнем виде.
        if (frame.editor.grid && i + 1 == frame.views.size()) {
            m_grid->draw(commands, viewUniforms);
        }
        commands.endRenderPass();
    }
    m_outline->draw(commands, frame, viewUniforms, color, &depth);
}

void Renderer::present()
{
    m_device->commands().transition(m_device->backBuffer(), rhi::ResourceState::Present);
    m_device->present();
    m_uniforms->reset();
    m_meshes->endFrame();
    m_uiTextures->endFrame();
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
