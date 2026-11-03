#pragma once

#include "yqlora/resource/ResourceLoader.hpp"

namespace yqlora {

// Материалы проекта (.ymat) в MaterialData. Шейдер материала - ссылка по
// ID: его загружает и проверяет тот, кто рисует.
class YQLORA_API MaterialLoader final : public ResourceLoader {
public:
    std::span<const std::string_view> extensions() const override;
    std::string_view kind() const override;
    LoadResult load(const LoadRequest& request) const override;
};

} // namespace yqlora
