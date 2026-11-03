#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/rhi/Texture.hpp"

namespace yqlora {

// Сэмплер видеопамяти по настройкам импорта текстуры: его берут и превью в
// интерфейсе, и материалы.
YQLORA_API rhi::SamplerDesc samplerDesc(const TextureSampling& sampling);

} // namespace yqlora
