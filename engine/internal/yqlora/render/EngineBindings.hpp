#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/RenderGraph.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/rhi/CommandList.hpp"
#include "yqlora/rhi/Pipeline.hpp"

#include <optional>
#include <vector>

namespace yqlora {

// Ресурсы движка в наборах кадра и вида (uniforms.glsl, atmosphere.glsl):
// блоки Frame, Atmosphere, View и таблицы атмосферы. Пустой слайс или
// nullptr - этого ресурса нет у того, кто привязывает.
struct EngineResources {
    UniformSlice frame{};
    UniformSlice atmosphere{};
    UniformSlice view{};
    const rhi::Texture* transmittance = nullptr;
    const rhi::Texture* skyView = nullptr;
    const rhi::Texture* aerialPerspective = nullptr;
    // Линейный, по краю: таблицы читаются между текселями и без повтора.
    const rhi::Sampler* lutSampler = nullptr;
    // Свет неба (EnvironmentLighting) и его сэмплер: линейный, с мипами.
    const rhi::Texture* ambientHarmonics = nullptr;
    const rhi::Texture* environmentSpecular = nullptr;
    const rhi::Texture* environmentBrdf = nullptr;
    const rhi::Sampler* environmentSampler = nullptr;
    // Тени вида (ShadowPass): блок Shadows, карты и их сэмплеры - со
    // сравнением и глубина как есть.
    UniformSlice shadows{};
    const rhi::Texture* shadowCascades = nullptr;
    const rhi::Texture* shadowAtlas = nullptr;
    const rhi::Sampler* shadowCompareSampler = nullptr;
    const rhi::Sampler* shadowDepthSampler = nullptr;
};

// Текстуры движка в графе кадра; nullopt - этой текстуры нет.
struct EngineTextureIds {
    std::optional<RenderGraph::TextureId> transmittance{};
    std::optional<RenderGraph::TextureId> skyView{};
    std::optional<RenderGraph::TextureId> aerialPerspective{};
    std::optional<RenderGraph::TextureId> ambientHarmonics{};
    std::optional<RenderGraph::TextureId> environmentSpecular{};
    std::optional<RenderGraph::TextureId> environmentBrdf{};
    std::optional<RenderGraph::TextureId> shadowCascades{};
    std::optional<RenderGraph::TextureId> shadowAtlas{};

    // Что из них есть: читает проход, который их привязывает.
    YQLORA_API std::vector<RenderGraph::TextureId> reads() const;
};

// resources с текстурами ids из графа: только внутри record прохода, когда
// текстуры графа уже есть.
YQLORA_API EngineResources
withTextures(EngineResources resources, const RenderGraph& graph, const EngineTextureIds& ids);

// Задаёт привязанному конвейеру с шейдером shader те ресурсы движка,
// которые шейдер объявил: шейдеру без атмосферы её таблицы не нужны.
YQLORA_API void bindEngineResources(
    rhi::CommandList& commands,
    const rhi::Shader& shader,
    const EngineResources& resources
);

} // namespace yqlora
