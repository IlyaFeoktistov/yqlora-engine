#include "yqlora/opengl/GlDevice.hpp"

#include "yqlora/opengl/GlBuffer.hpp"
#include "yqlora/opengl/GlCommandList.hpp"
#include "yqlora/opengl/GlShader.hpp"
#include "yqlora/opengl/GlTexture.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/platform/Window.hpp"

#include <SDL3/SDL.h>
#include <glad/gl.h>

#include <stdexcept>
#include <string>

namespace yqlora::opengl {

namespace {

constexpr int kDepthBits = 24;
constexpr int kStencilBits = 8;

} // namespace

void GlDevice::prepareWindow()
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, kVersionMajor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, kVersionMinor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    // Как у целей рендера (Depth24Stencil8): одни конвейеры рисуют и туда, и в окно.
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, kDepthBits);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, kStencilBits);
}

GlDevice::GlDevice(Window& window)
    : m_window(window)
{
    m_context = SDL_GL_CreateContext(window.sdlWindow());
    if (!m_context) {
        throw std::runtime_error(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
    }

    const int version = gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));
    if (version == 0) {
        SDL_GL_DestroyContext(m_context);
        throw std::runtime_error("Failed to load OpenGL functions");
    }

    log::info(
        "OpenGL {}.{}: {} ({})",
        GLAD_VERSION_MAJOR(version),
        GLAD_VERSION_MINOR(version),
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
        reinterpret_cast<const char*>(glGetString(GL_VERSION))
    );

    GLint alignment = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    m_limits.uniformOffsetAlignment = static_cast<std::size_t>(alignment);

    const int width = window.widthInPixels();
    const int height = window.heightInPixels();
    m_backBuffer = GlTexture::backBuffer(
        *this,
        {.width = width, .height = height, .format = rhi::Format::Rgba8, .usage = rhi::kColorTarget}
    );
    m_backBufferDepth = GlTexture::backBuffer(
        *this,
        {.width = width, .height = height, .format = rhi::Format::Depth24Stencil8, .usage = rhi::kDepthTarget}
    );
    // Между кадрами задний буфер показывается на экране.
    m_commands = std::make_unique<GlCommandList>(*this);
    m_commands->transition(*m_backBuffer, rhi::ResourceState::Present);
}

GlDevice::~GlDevice()
{
    for (const auto& [attachments, framebuffer] : m_framebuffers) {
        glDeleteFramebuffers(1, &framebuffer);
    }
    m_framebuffers.clear();
    m_commands.reset();
    m_backBufferDepth.reset();
    m_backBuffer.reset();
    SDL_GL_DestroyContext(m_context);
}

std::unique_ptr<rhi::Buffer> GlDevice::createBuffer(const rhi::BufferDesc& desc, std::span<const std::byte> data)
{
    return std::make_unique<GlBuffer>(desc, data);
}

std::unique_ptr<rhi::Texture> GlDevice::createTexture(const rhi::TextureDesc& desc, std::span<const std::byte> pixels)
{
    return std::make_unique<GlTexture>(*this, desc, pixels);
}

std::unique_ptr<rhi::Sampler> GlDevice::createSampler(const rhi::SamplerDesc& desc)
{
    return std::make_unique<GlSampler>(desc);
}

std::unique_ptr<rhi::Shader> GlDevice::createShader(rhi::ShaderDesc desc)
{
    return std::make_unique<GlShader>(std::move(desc));
}

std::unique_ptr<rhi::Pipeline> GlDevice::createPipeline(rhi::PipelineDesc desc)
{
    // Состояние выставляет команда bindPipeline, своего объекта в OpenGL нет.
    if (!dynamic_cast<const GlShader*>(desc.shader)) {
        throw std::logic_error("createPipeline: the shader is not made by this device");
    }

    return std::make_unique<rhi::Pipeline>(std::move(desc));
}

rhi::Texture& GlDevice::backBuffer()
{
    return *m_backBuffer;
}

rhi::Texture& GlDevice::backBufferDepth()
{
    return *m_backBufferDepth;
}

void GlDevice::resizeBackBuffer(int width, int height)
{
    m_backBuffer->resizeBackBuffer(width, height);
    m_backBufferDepth->resizeBackBuffer(width, height);
}

rhi::CommandList& GlDevice::commands()
{
    return *m_commands;
}

void GlDevice::present()
{
    SDL_GL_SwapWindow(m_window.sdlWindow());
}

bool GlDevice::setVsync(bool enabled)
{
    if (!SDL_GL_SetSwapInterval(enabled ? 1 : 0)) {
        log::warn("VSync cannot be switched: {}", SDL_GetError());

        return false;
    }

    return true;
}

unsigned int GlDevice::framebuffer(const GlTexture& color, const GlTexture* depth)
{
    const std::pair key(color.id(), depth ? depth->id() : 0U);
    if (const auto it = m_framebuffers.find(key); it != m_framebuffers.end()) {
        return it->second;
    }

    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color.id(), 0);
    if (depth) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth->id(), 0);
    }
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &framebuffer);
        throw std::runtime_error("Render pass framebuffer is incomplete");
    }
    m_framebuffers.emplace(key, framebuffer);

    return framebuffer;
}

void GlDevice::forgetFramebuffers(unsigned int texture)
{
    for (auto it = m_framebuffers.begin(); it != m_framebuffers.end();) {
        if (it->first.first == texture || it->first.second == texture) {
            glDeleteFramebuffers(1, &it->second);
            it = m_framebuffers.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace yqlora::opengl
