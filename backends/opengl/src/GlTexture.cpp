#include "yqlora/opengl/GlTexture.hpp"

#include "yqlora/opengl/GlDevice.hpp"

#include <glad/gl.h>

#include <stdexcept>

namespace yqlora::opengl {

namespace {

struct GlFormat {
    GLint internalFormat = GL_RGBA8;
    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;
};

GlFormat glFormat(rhi::Format format)
{
    switch (format) {
    case rhi::Format::Rgba8: return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case rhi::Format::Rgba8Srgb: return {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case rhi::Format::R8: return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
    case rhi::Format::Depth24Stencil8: return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
    case rhi::Format::None: break;
    }
    throw std::logic_error("Texture without a format");
}

GLint wrapMode(rhi::Wrap wrap)
{
    switch (wrap) {
    case rhi::Wrap::Repeat: return GL_REPEAT;
    case rhi::Wrap::Clamp: return GL_CLAMP_TO_EDGE;
    case rhi::Wrap::Mirror: return GL_MIRRORED_REPEAT;
    }

    return GL_REPEAT;
}

GLint minFilter(const rhi::SamplerDesc& sampling)
{
    if (sampling.filter == rhi::Filter::Nearest) {
        return sampling.mipmaps ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
    }

    return sampling.mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
}

GLint magFilter(const rhi::SamplerDesc& sampling)
{
    return sampling.filter == rhi::Filter::Nearest ? GL_NEAREST : GL_LINEAR;
}

} // namespace

GlTexture::GlTexture(GlDevice& device, const rhi::TextureDesc& desc, std::span<const std::byte> pixels)
    : rhi::Texture(desc)
    , m_device(device)
{
    const GlFormat format = glFormat(desc.format);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    // Строки в pixels плотные; по умолчанию OpenGL ждёт строки по 4 байта.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // Строки идут сверху вниз и попадают в текстуру первой строкой при v = 0:
    // это и есть uv0 = (0, 0) в левом верхнем углу, переворачивать не нужно.
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format.internalFormat,
        desc.width,
        desc.height,
        0,
        format.format,
        format.type,
        pixels.empty() ? nullptr : pixels.data()
    );
    if (desc.mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    // Без мипмапов фильтр по умолчанию (с мипмапами) делает текстуру неполной.
    setOwnSampling({.filter = rhi::Filter::Linear, .wrap = rhi::Wrap::Clamp, .mipmaps = desc.mipmaps});
}

GlTexture::GlTexture(GlDevice& device, const rhi::TextureDesc& desc, BackBufferTag)
    : rhi::Texture(desc)
    , m_device(device)
{
}

std::unique_ptr<GlTexture> GlTexture::backBuffer(GlDevice& device, const rhi::TextureDesc& desc)
{
    return std::unique_ptr<GlTexture>(new GlTexture(device, desc, BackBufferTag{}));
}

GlTexture::~GlTexture()
{
    if (m_texture != 0) {
        m_device.forgetFramebuffers(m_texture);
        glDeleteTextures(1, &m_texture);
    }
}

void GlTexture::setOwnSampling(const rhi::SamplerDesc& sampling)
{
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode(sampling.wrap));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode(sampling.wrap));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter(sampling));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter(sampling));
    glBindTexture(GL_TEXTURE_2D, 0);
}

GlSampler::GlSampler(const rhi::SamplerDesc& desc)
    : rhi::Sampler(desc)
{
    glGenSamplers(1, &m_sampler);
    glSamplerParameteri(m_sampler, GL_TEXTURE_WRAP_S, wrapMode(desc.wrap));
    glSamplerParameteri(m_sampler, GL_TEXTURE_WRAP_T, wrapMode(desc.wrap));
    glSamplerParameteri(m_sampler, GL_TEXTURE_MIN_FILTER, minFilter(desc));
    glSamplerParameteri(m_sampler, GL_TEXTURE_MAG_FILTER, magFilter(desc));
}

GlSampler::~GlSampler()
{
    glDeleteSamplers(1, &m_sampler);
}

} // namespace yqlora::opengl
