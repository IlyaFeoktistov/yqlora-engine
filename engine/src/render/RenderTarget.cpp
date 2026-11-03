#include "yqlora/render/RenderTarget.hpp"

#include "yqlora/render/FrameFormats.hpp"

#include <algorithm>
#include <stdexcept>

namespace yqlora {

namespace {

// Интерфейс показывает кадр один в один, без повторов и mip-уровней.
constexpr rhi::SamplerDesc kUiSampling{.filter = rhi::Filter::Linear, .wrap = rhi::Wrap::Clamp, .mipmaps = false};

} // namespace

RenderTarget::RenderTarget(rhi::Device& device, UiIntegration* ui, int width, int height)
    : m_device(device)
    , m_ui(ui)
{
    allocate(std::max(width, 1), std::max(height, 1));
}

void RenderTarget::resize(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == this->width() && height == this->height()) {
        return;
    }
    allocate(width, height);
}

UiTexture RenderTarget::uiTexture()
{
    if (!m_ui) {
        throw std::logic_error("RenderTarget::uiTexture: the renderer has no interface");
    }

    return m_ui->texture(*m_color, kUiSampling);
}

void RenderTarget::allocate(int width, int height)
{
    m_color = m_device.createTexture({
        .width = width,
        .height = height,
        .format = kOutputColorFormat,
        .usage = rhi::kColorTarget | rhi::kSampled,
    });
    m_depth = m_device.createTexture({
        .width = width,
        .height = height,
        .format = kDepthFormat,
        .usage = rhi::kDepthTarget,
    });
}

} // namespace yqlora
