#pragma once

#include "yqlora/rhi/RhiTypes.hpp"

namespace yqlora {

// Форматы целей кадра. Виды рисуются в HDR-цель, проход вывода переводит её
// в итоговую цель: окно или RenderTarget. Итоговая цель - в форматах окна,
// поэтому одни конвейеры рисуют и туда, и туда; глубина у сцены и итоговой
// цели общая (её проверяют проходы владельцев видов).
inline constexpr rhi::Format kHdrColorFormat = rhi::Format::Rgba16Float;
inline constexpr rhi::Format kOutputColorFormat = rhi::kBackBufferColorFormat;
inline constexpr rhi::Format kDepthFormat = rhi::kBackBufferDepthFormat;

} // namespace yqlora
