#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/EngineBindings.hpp"
#include "yqlora/render/FrameData.hpp"

namespace yqlora {

struct RenderContext;

// Небо окружения кадра (EnvironmentData::sky) в открытый проход вида: в
// HDR-цель кадра на дальней плоскости, только там, где ничего не
// нарисовано. Рисуется после непрозрачного и до прозрачного. Неба нет или
// его шейдер не собрался - ничего не рисует.
YQLORA_API void drawSky(
    rhi::CommandList& commands,
    const RenderContext& context,
    const FrameData& frame,
    const EngineResources& resources
);

} // namespace yqlora
