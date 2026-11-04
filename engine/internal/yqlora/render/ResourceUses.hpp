#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <string_view>

namespace yqlora {

class ResourceCache;

// Годится ли ресурс id назначению use (uses::kSkyMaterial): пустое
// назначение - любой ресурс. Ресурс загружается, если ещё не загружен.
// Материал, чей шейдер не загрузился (ошибка компиляции), годится и небу, и
// поверхности: иначе его не выбрать, пока шейдер не исправлен.
YQLORA_API bool fitsResourceUse(std::string_view use, ResourceId id, ResourceCache& cache);

} // namespace yqlora
