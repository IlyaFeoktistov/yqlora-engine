#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/rhi/CommandList.hpp"
#include "yqlora/rhi/Pipeline.hpp"

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
};

// Задаёт привязанному конвейеру с шейдером shader те ресурсы движка,
// которые шейдер объявил: шейдеру без атмосферы её таблицы не нужны.
YQLORA_API void bindEngineResources(
    rhi::CommandList& commands,
    const rhi::Shader& shader,
    const EngineResources& resources
);

} // namespace yqlora
