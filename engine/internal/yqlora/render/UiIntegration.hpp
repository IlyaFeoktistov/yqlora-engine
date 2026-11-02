#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/render/UiRenderer.hpp"
#include "yqlora/rhi/Texture.hpp"

#include <cstdint>
#include <memory>

namespace yqlora {

// Текстура для показа в интерфейсе (ImGui::Image). uv0 и uv1 задают углы:
// у OpenGL нарисованная картинка в текстуре перевёрнута по вертикали, у
// других API нет.
struct UiTexture {
    std::uint64_t id = 0;
    Vec2 uv0{0.0f, 0.0f};
    Vec2 uv1{1.0f, 1.0f};
};

// Dear ImGui на графическом API. Не часть RHI: рисует тем же устройством, но
// RHI от неё не зависит, а игре без интерфейса она не нужна. Реализация -
// в бэкенде, создаёт её createUiIntegration (GraphicsBackend.hpp).
class UiIntegration {
public:
    virtual ~UiIntegration() = default;

    // После ImGui::CreateContext; удаляется до ImGui::DestroyContext.
    virtual std::unique_ptr<UiRenderer> createUiRenderer() = 0;

    // Текстура устройства для ImGui::Image, читается с sampling. Текстура
    // в состоянии ShaderRead и живёт, пока интерфейс её рисует.
    virtual UiTexture texture(rhi::Texture& texture, const rhi::SamplerDesc& sampling) = 0;
};

} // namespace yqlora
