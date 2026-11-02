#include "yqlora/render/GraphicsBackend.hpp"

#include "yqlora/platform/Window.hpp"

#if YQLORA_HAS_OPENGL
#include "yqlora/opengl/GlDevice.hpp"
#include "yqlora/opengl/GlUiIntegration.hpp"
#endif

#include <stdexcept>
#include <string>

namespace yqlora {

namespace {

[[noreturn]] void throwNotBuilt(GraphicsApi api)
{
    throw std::runtime_error(std::string("Renderer backend is not built: ") + std::string(graphicsApiName(api)));
}

} // namespace

bool isBuilt(GraphicsApi api)
{
    switch (api) {
    case GraphicsApi::OpenGL: return YQLORA_HAS_OPENGL != 0;
    case GraphicsApi::Vulkan:
    case GraphicsApi::Direct3D12: return false;
    }

    return false;
}

void prepareWindowForApi(GraphicsApi api)
{
    switch (api) {
    case GraphicsApi::OpenGL:
#if YQLORA_HAS_OPENGL
        opengl::GlDevice::prepareWindow();

        return;
#else
        break;
#endif
    case GraphicsApi::Vulkan:
    case GraphicsApi::Direct3D12: break;
    }
    throwNotBuilt(api);
}

std::unique_ptr<rhi::Device> createDevice(GraphicsApi api, [[maybe_unused]] Window& window)
{
    switch (api) {
    case GraphicsApi::OpenGL:
#if YQLORA_HAS_OPENGL

        return std::make_unique<opengl::GlDevice>(window);
#else
        break;
#endif
    case GraphicsApi::Vulkan:
    case GraphicsApi::Direct3D12: break;
    }
    throwNotBuilt(api);
}

std::unique_ptr<UiIntegration> createUiIntegration(
    GraphicsApi api,
    [[maybe_unused]] Window& window,
    [[maybe_unused]] rhi::Device& device
)
{
    switch (api) {
    case GraphicsApi::OpenGL:
#if YQLORA_HAS_OPENGL

        return std::make_unique<opengl::GlUiIntegration>(window.sdlWindow(), static_cast<opengl::GlDevice&>(device));
#else
        break;
#endif
    case GraphicsApi::Vulkan:
    case GraphicsApi::Direct3D12: break;
    }
    throwNotBuilt(api);
}

} // namespace yqlora
