#pragma once

#include "yqlora/resource/ResourceId.hpp"

namespace yqlora::builtin {

// Ресурсы, встроенные в движок. ID у них постоянные и маленькие,
// ID ресурсов проекта начинаются с kFirstProjectId и с ними не пересекаются.
// Сами меши - в BuiltinMeshes.hpp, шейдеры - в BuiltinShaders.hpp.
inline constexpr ResourceId kCubeMesh = 1;
inline constexpr ResourceId kSphereMesh = 2;
inline constexpr ResourceId kPlaneMesh = 3;

// Освещённый шейдер PBR, с которого начинается каждый новый материал.
inline constexpr ResourceId kStandardShader = 0x101;
// Пурпурный: им рисуется материал, чей шейдер не компилируется.
inline constexpr ResourceId kErrorShader = 0x102;

inline constexpr ResourceId kFirstProjectId = 1024;

inline bool isBuiltin(ResourceId id)
{
    return id != 0 && id < kFirstProjectId;
}

} // namespace yqlora::builtin
