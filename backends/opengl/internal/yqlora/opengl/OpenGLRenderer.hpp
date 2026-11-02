#pragma once

#include "yqlora/render/GpuResourceCache.hpp"
#include "yqlora/render/Renderer.hpp"

#include <memory>

namespace yqlora {
class GpuMeshCache;
class UniformRing;
} // namespace yqlora

namespace yqlora::rhi {
class Texture;
}

namespace yqlora::opengl {

class GlDevice;
class GridPass;
class MeshPass;
class OutlinePass;

// Рендерер поверх rhi::Device на OpenGL. Проходы уже пишут только команды
// RHI; в движок они переезжают в R.3.
class OpenGLRenderer final : public Renderer {
public:
    explicit OpenGLRenderer(Window& window);
    ~OpenGLRenderer() override;

    OpenGLRenderer(const OpenGLRenderer&) = delete;
    OpenGLRenderer& operator=(const OpenGLRenderer&) = delete;

    GraphicsApi api() const override
    {
        return GraphicsApi::OpenGL;
    }

    DepthRange depthRange() const override
    {
        return DepthRange::MinusOneToOne;
    }

    void render(const FrameData& frame) override;
    void render(const FrameData& frame, RenderTarget& target) override;
    std::unique_ptr<RenderTarget> createRenderTarget(int width, int height) override;
    void clearWindow(const Vec4& color) override;

    UiTexture uiTexture(ResourceId id, std::uint64_t revision, const TextureData& data) override;
    std::unique_ptr<UiRenderer> createUiRenderer() override;

    void present() override;
    void resize(int width, int height) override;
    bool setVsync(bool enabled) override;

private:
    void drawScene(const FrameData& frame, rhi::Texture& color, rhi::Texture& depth);

    Window& m_window;
    // Первым создаётся и последним удаляется: остальное - объекты устройства.
    std::unique_ptr<GlDevice> m_device;
    std::unique_ptr<UniformRing> m_uniforms;
    std::unique_ptr<GpuMeshCache> m_meshes;
    std::unique_ptr<GpuResourceCache<rhi::Texture>> m_uiTextures;
    std::unique_ptr<MeshPass> m_meshPass;
    std::unique_ptr<GridPass> m_grid;
    std::unique_ptr<OutlinePass> m_outline;
};

} // namespace yqlora::opengl
