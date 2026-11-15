#pragma once

#include "yqlora/resource/ResourceLoader.hpp"

namespace yqlora {

// Физические материалы проекта (.yphysmat) в PhysicsMaterialData.
class YQLORA_API PhysicsMaterialLoader final : public ResourceLoader {
public:
    std::span<const std::string_view> extensions() const override;
    std::string_view kind() const override;
    LoadResult load(const LoadRequest& request) const override;
};

} // namespace yqlora
