#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/rhi/RhiTypes.hpp"

#include <memory>
#include <optional>

namespace yqlora {

class AtmospherePass;
class EnvironmentLighting;
class MeshPass;
class OutputPass;
class TransientTextures;
struct RenderContext;

namespace rhi {
class Texture;
} // namespace rhi

// Граф кадра сцены: таблицы атмосферы, виды по очереди в линейную HDR-цель
// (непрозрачное, небо, прозрачное), проход вывода в итоговую цель, затем
// проходы владельцев видов. Устройство, показ кадра и
// интерфейс - у Renderer; здесь только то, что и в каком порядке рисуется.
class YQLORA_API SceneRenderer {
public:
    SceneRenderer(const RenderContext& context, TransientTextures& transients);
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    // Записывает кадр в команды устройства. color и depth - итоговая цель;
    // finalColorState - в каком состоянии оставить цвет (nullopt - как был).
    void draw(
        const FrameData& frame,
        rhi::Texture& color,
        rhi::Texture& depth,
        std::optional<rhi::ResourceState> finalColorState
    );

private:
    const RenderContext& m_context;
    TransientTextures& m_transients;
    std::unique_ptr<AtmospherePass> m_atmosphere;
    std::unique_ptr<EnvironmentLighting> m_environment;
    std::unique_ptr<MeshPass> m_meshPass;
    std::unique_ptr<OutputPass> m_outputPass;
};

} // namespace yqlora
