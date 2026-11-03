#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/TextureData.hpp"

#include <nlohmann/json.hpp>

namespace yqlora {

// Настройки импорта текстуры из объекта JSON .meta. Ключа нет - значение по
// умолчанию; неизвестное значение - тоже, с предупреждением в логе.
YQLORA_API TextureSampling textureSamplingFromJson(const nlohmann::json& import);

// Объект JSON для .meta; значения по умолчанию не пишутся.
YQLORA_API nlohmann::json toJson(const TextureSampling& sampling);

} // namespace yqlora
