#pragma once

#include "yqlora/render/UiIntegration.hpp"

struct SDL_Window;

namespace yqlora::opengl {

class GlDevice;

// Dear ImGui на OpenGL: отрисовщик интерфейса и текстуры для ImGui::Image.
class GlUiIntegration final : public UiIntegration {
public:
    GlUiIntegration(SDL_Window* window, GlDevice& device)
        : m_window(window)
        , m_device(device)
    {
    }

    std::unique_ptr<UiRenderer> createUiRenderer() override;

    // ImGui сэмплеров не знает и читает текстуру с её собственными
    // параметрами, поэтому sampling задаётся самой текстуре.
    UiTexture texture(rhi::Texture& texture, const rhi::SamplerDesc& sampling) override;

private:
    SDL_Window* m_window;
    GlDevice& m_device;
};

} // namespace yqlora::opengl
