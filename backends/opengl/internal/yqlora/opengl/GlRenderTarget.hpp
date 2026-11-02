#pragma once

#include "yqlora/render/RenderTarget.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>

namespace yqlora::opengl {

// Цель рендера: текстура цвета и своя глубина. После отрисовки цвет в
// состоянии ShaderRead: его читает интерфейс.
class GlRenderTarget final : public RenderTarget {
public:
    GlRenderTarget(rhi::Device& device, int width, int height);

    int width() const override
    {
        return m_color->desc().width;
    }

    int height() const override
    {
        return m_color->desc().height;
    }

    void resize(int width, int height) override;
    UiTexture uiTexture() const override;

    rhi::Texture& color() const
    {
        return *m_color;
    }

    rhi::Texture& depth() const
    {
        return *m_depth;
    }

private:
    void allocate(int width, int height);

    rhi::Device& m_device;
    std::unique_ptr<rhi::Texture> m_color;
    std::unique_ptr<rhi::Texture> m_depth;
};

} // namespace yqlora::opengl
