#pragma once

#include "yqlora/reflection/EnumNames.hpp"

namespace yqlora {

enum class GraphicsApi {
    OpenGL,
    Vulkan,
    Direct3D12,
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::GraphicsApi, "OpenGL", "Vulkan", "Direct3D12")
