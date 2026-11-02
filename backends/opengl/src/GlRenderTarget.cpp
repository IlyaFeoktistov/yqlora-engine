#include "yqlora/opengl/GlRenderTarget.hpp"

#include "yqlora/opengl/GlTexture.hpp"

#include <algorithm>

namespace yqlora::opengl {

GlRenderTarget::GlRenderTarget(rhi::Device& device, int width, int height)
    : m_device(device)
{
    allocate(std::max(width, 1), std::max(height, 1));
}

void GlRenderTarget::resize(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == this->width() && height == this->height()) {
        return;
    }
    allocate(width, height);
}

UiTexture GlRenderTarget::uiTexture() const
{
    // OpenGL хранит первую строку текстуры внизу, интерфейс ждёт её сверху.
    return {static_cast<const GlTexture&>(*m_color).id(), {0.0f, 1.0f}, {1.0f, 0.0f}};
}

void GlRenderTarget::allocate(int width, int height)
{
    m_color = m_device.createTexture({
        .width = width,
        .height = height,
        .format = rhi::Format::Rgba8,
        .usage = rhi::kColorTarget | rhi::kSampled,
    });
    m_depth = m_device.createTexture({
        .width = width,
        .height = height,
        .format = rhi::Format::Depth24Stencil8,
        .usage = rhi::kDepthTarget,
    });
}

} // namespace yqlora::opengl
