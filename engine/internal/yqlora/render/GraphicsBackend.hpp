#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/GraphicsApi.hpp"
#include "yqlora/render/UiIntegration.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>

namespace yqlora {

class Window;

// Бэкенды графических API: что собрано и как создать их части. Только
// здесь движок знает о конкретных API.

// Собран ли бэкенд API в этой сборке движка.
YQLORA_API bool isBuilt(GraphicsApi api);

// Настраивает параметры, которые API требует задать до создания окна
// (например, версию контекста OpenGL). Вызывается после platform::initialize().
YQLORA_API void prepareWindowForApi(GraphicsApi api);

// Бросают std::runtime_error, если API не собран в этой сборке или не
// поддерживается системой.
YQLORA_API std::unique_ptr<rhi::Device> createDevice(GraphicsApi api, Window& window);
YQLORA_API std::unique_ptr<UiIntegration> createUiIntegration(GraphicsApi api, Window& window, rhi::Device& device);

} // namespace yqlora
