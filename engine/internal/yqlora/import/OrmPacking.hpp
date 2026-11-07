#pragma once

#include "yqlora/render/TextureData.hpp"

#include <optional>

namespace yqlora {

// Карта ormMap из отдельных картинок: R - затенение (из R occlusion), G и B -
// шероховатость и металличность (из G и B metallicRoughness), альфа 255.
// Нет картинки - её каналы 255 (множитель 1). Размер - больший из двух,
// меньшая картинка растягивается билинейно. Сэмплинг - metallicRoughness,
// если она есть, иначе occlusion; цветовое пространство линейное. nullopt,
// если обеих нет или картинка не 8 бит на канал.
std::optional<TextureData> packOrm(const TextureData* occlusion, const TextureData* metallicRoughness);

} // namespace yqlora
