#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/material/MaterialData.hpp"
#include "yqlora/material/MaterialOverrides.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

namespace yqlora {

// Байты блока Material шейдера shader по смещениям его отражения:
// значения материала (или default полей), поверх них замены overrides.
// Цвета (пометка color) переводятся из sRGB в линейные, альфа как есть;
// int и uint пишутся целыми. Замена, которой нет у шейдера или не того типа,
// пропускается: о ней сообщает skipped(имя). Пусто, если блока нет.
YQLORA_API std::vector<std::byte> materialBlock(
    const ShaderData& shader,
    const MaterialData& material,
    const MaterialOverrides* overrides = nullptr,
    const std::function<void(std::string_view)>& skipped = {}
);

} // namespace yqlora
