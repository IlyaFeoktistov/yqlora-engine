#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/UiIntegration.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>

namespace yqlora {

// Текстура, в которую рисуется кадр вместо окна: цвет и своя глубина.
// Например, окно сцены в редакторе. После отрисовки цвет в состоянии
// ShaderRead: его читает интерфейс.
class YQLORA_API RenderTarget {
public:
    // ui - связка с интерфейсом; без неё uiTexture() недоступна.
    RenderTarget(rhi::Device& device, UiIntegration* ui, int width, int height);

    int width() const
    {
        return m_color->desc().width;
    }

    int height() const
    {
        return m_color->desc().height;
    }

    // Размер в пикселях. Если он не изменился, ничего не делает.
    void resize(int width, int height);

    UiTexture uiTexture();

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
    UiIntegration* m_ui = nullptr;
    std::unique_ptr<rhi::Texture> m_color;
    std::unique_ptr<rhi::Texture> m_depth;
};

} // namespace yqlora
