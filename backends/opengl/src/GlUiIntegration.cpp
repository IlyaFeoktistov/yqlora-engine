#include "yqlora/opengl/GlUiIntegration.hpp"

#include "yqlora/opengl/GlDevice.hpp"
#include "yqlora/opengl/GlTexture.hpp"
#include "yqlora/opengl/GlUiRenderer.hpp"

namespace yqlora::opengl {

std::unique_ptr<UiRenderer> GlUiIntegration::createUiRenderer()
{
    return std::make_unique<GlUiRenderer>(m_window, m_device.context());
}

UiTexture GlUiIntegration::texture(rhi::Texture& texture, const rhi::SamplerDesc& sampling)
{
    auto& glTexture = static_cast<GlTexture&>(texture);
    glTexture.setOwnSampling(sampling);
    // Загруженная картинка лежит первой строкой при v = 0, как ждёт интерфейс
    // (см. GlTexture). Нарисованная - снизу вверх: OpenGL рисует с осью y вверх.
    if ((texture.desc().usage & rhi::kColorTarget) != 0U) {
        return {glTexture.id(), {0.0f, 1.0f}, {1.0f, 0.0f}};
    }

    return {glTexture.id(), {0.0f, 0.0f}, {1.0f, 1.0f}};
}

} // namespace yqlora::opengl
