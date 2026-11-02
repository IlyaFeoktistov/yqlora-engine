#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/GpuResourceCache.hpp"
#include "yqlora/render/GraphicsApi.hpp"
#include "yqlora/render/RenderTarget.hpp"
#include "yqlora/render/UiIntegration.hpp"
#include "yqlora/render/UiRenderer.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>

namespace yqlora {

class GpuMeshCache;
class GridPass;
class MeshPass;
class OutlinePass;
class ShaderCompiler;
class UniformRing;
class Window;
struct TextureData;

// Рендерер движка: проходы над rhi::Device одного графического API. Своих
// классов под API нет, бэкенд даёт только устройство и связку с интерфейсом.
class YQLORA_API Renderer {
public:
    // ui может быть nullptr: рендерер без интерфейса (тесты, запуск без окна
    // ImGui). Объекты устройства удаляются раньше него.
    Renderer(GraphicsApi api, std::unique_ptr<rhi::Device> device, std::unique_ptr<UiIntegration> ui);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    GraphicsApi api() const
    {
        return m_api;
    }

    rhi::Device& device() const
    {
        return *m_device;
    }

    // Рисует кадр в окно. На экране он появится после present(): между ними
    // поверх можно дорисовать интерфейс.
    void render(const FrameData& frame);

    // Рисует кадр в текстуру. Камеру кадра стоит строить с соотношением сторон цели.
    void render(const FrameData& frame, RenderTarget& target);

    std::unique_ptr<RenderTarget> createRenderTarget(int width, int height);

    // Заливает окно цветом. Нужно, когда сцена рисуется в текстуру, а окно
    // целиком занято интерфейсом.
    void clearWindow(const Vec4& color);

    // Текстура ресурса для показа в интерфейсе (превью в редакторе), с
    // цветами как в файле. Копия в видеопамяти живёт по ID и ревизии, см.
    // GpuResourceCache; брать заново каждый кадр.
    UiTexture uiTexture(ResourceId id, std::uint64_t revision, const TextureData& data);

    // Отрисовка Dear ImGui под этот графический API, см. UiRenderer.
    std::unique_ptr<UiRenderer> createUiRenderer();

    // Показывает нарисованный кадр на экране.
    void present();

    // Размер области вывода в пикселях изменился.
    void resize(int width, int height);

    // Ждать ли обновления экрана перед показом кадра. false, если система не даёт его включить.
    bool setVsync(bool enabled);

private:
    void drawScene(const FrameData& frame, rhi::Texture& color, rhi::Texture& depth);
    UiIntegration& ui() const;

    GraphicsApi m_api;
    // Первым создаётся и последним удаляется: остальное - объекты устройства.
    std::unique_ptr<rhi::Device> m_device;
    std::unique_ptr<UiIntegration> m_ui;
    std::unique_ptr<ShaderCompiler> m_shaders;
    std::unique_ptr<UniformRing> m_uniforms;
    std::unique_ptr<GpuMeshCache> m_meshes;
    std::unique_ptr<GpuResourceCache<rhi::Texture>> m_uiTextures;
    std::unique_ptr<MeshPass> m_meshPass;
    std::unique_ptr<GridPass> m_grid;
    std::unique_ptr<OutlinePass> m_outline;
};

// Рендерер для окна на бэкенде api (см. GraphicsBackend.hpp). Бросает
// std::runtime_error, если API не собран или не поддерживается системой.
YQLORA_API std::unique_ptr<Renderer> createRenderer(GraphicsApi api, Window& window);

} // namespace yqlora
