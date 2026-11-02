#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/GraphicsApi.hpp"
#include "yqlora/render/RenderTarget.hpp"
#include "yqlora/render/UiRenderer.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <memory>

namespace yqlora {

class Window;
struct TextureData;

// Контракт движка на отрисовку. Реализации живут в backends/<api>/
// и не светят заголовки своего API наружу.
class YQLORA_API Renderer {
public:
    virtual ~Renderer() = default;

    virtual GraphicsApi api() const = 0;

    // Рисует кадр в окно. На экране он появится после present(): между ними
    // поверх можно дорисовать интерфейс.
    virtual void render(const FrameData& frame) = 0;

    // Рисует кадр в текстуру. Камеру кадра стоит строить с соотношением сторон цели.
    virtual void render(const FrameData& frame, RenderTarget& target) = 0;

    virtual std::unique_ptr<RenderTarget> createRenderTarget(int width, int height) = 0;

    // Заливает окно цветом. Нужно, когда сцена рисуется в текстуру, а окно
    // целиком занято интерфейсом.
    virtual void clearWindow(const Vec4& color) = 0;

    // Текстура ресурса для показа в интерфейсе (превью в редакторе), с
    // цветами как в файле. Копия в видеопамяти живёт по ID и ревизии, см.
    // GpuResourceCache; брать заново каждый кадр.
    virtual UiTexture uiTexture(ResourceId id, std::uint64_t revision, const TextureData& data) = 0;

    // Отрисовка Dear ImGui под этот графический API, см. UiRenderer.
    virtual std::unique_ptr<UiRenderer> createUiRenderer() = 0;

    // Показывает нарисованный кадр на экране.
    virtual void present() = 0;

    // Размер области вывода в пикселях изменился.
    virtual void resize(int width, int height) = 0;

    // Ждать ли обновления экрана перед показом кадра. false, если система не даёт его включить.
    virtual bool setVsync(bool enabled) = 0;
};

// Собран ли бэкенд API в этой сборке движка.
YQLORA_API bool isBuilt(GraphicsApi api);

// Настраивает параметры, которые API требует задать до создания окна
// (например, версию контекста OpenGL). Вызывается после platform::initialize().
YQLORA_API void prepareWindowForApi(GraphicsApi api);

// Создаёт рендерер для окна. Бросает std::runtime_error, если API
// не собран в этой сборке или не поддерживается системой.
YQLORA_API std::unique_ptr<Renderer> createRenderer(GraphicsApi api, Window& window);

} // namespace yqlora
