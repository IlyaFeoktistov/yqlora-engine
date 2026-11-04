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
    case rhi::Format::Rgba16Float: return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
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
    const GLenum bindTarget = target();
    glGenTextures(1, &m_texture);
    glBindTexture(bindTarget, m_texture);
    // Строки в pixels плотные; по умолчанию OpenGL ждёт строки по 4 байта.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // Строки идут сверху вниз и попадают в текстуру первой строкой при v = 0:
    // это и есть uv0 = (0, 0) в левом верхнем углу, переворачивать не нужно.
    // У кубической - каждая грань, пикселей нет: её рисуют.
    const int faces = desc.cube ? rhi::kCubeFaces : 1;
    for (int face = 0; face < faces; ++face) {
        glTexImage2D(
            desc.cube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face) : GL_TEXTURE_2D,
            0,
            format.internalFormat,
            desc.width,
            desc.height,
            0,
            format.format,
            format.type,
            pixels.empty() || desc.cube ? nullptr : pixels.data()
        );
    }
    if (desc.mipmaps) {
        glGenerateMipmap(bindTarget);
    }
    // Сэмплер с мипмапами читает мипы, только если они есть (контракт
    // rhi::SamplerDesc): без этого OpenGL считал бы текстуру без мипов
    // неполной и читал бы нули.
    glTexParameteri(bindTarget, GL_TEXTURE_MAX_LEVEL, rhi::mipLevelCount(desc) - 1);
    glBindTexture(bindTarget, 0);
    // Без мипмапов фильтр по умолчанию (с мипмапами) делает текстуру неполной.
    setOwnSampling({.filter = rhi::Filter::Linear, .wrap = rhi::Wrap::Clamp, .mipmaps = desc.mipmaps});
}

unsigned int GlTexture::target() const
{
    return desc().cube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
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
    const GLenum bindTarget = target();
    glBindTexture(bindTarget, m_texture);
    glTexParameteri(bindTarget, GL_TEXTURE_WRAP_S, wrapMode(sampling.wrap));
    glTexParameteri(bindTarget, GL_TEXTURE_WRAP_T, wrapMode(sampling.wrap));
    glTexParameteri(bindTarget, GL_TEXTURE_MIN_FILTER, minFilter(sampling));
    glTexParameteri(bindTarget, GL_TEXTURE_MAG_FILTER, magFilter(sampling));
    glBindTexture(bindTarget, 0);
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
