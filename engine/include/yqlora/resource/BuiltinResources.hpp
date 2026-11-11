#pragma once

#include "yqlora/resource/ResourceId.hpp"

namespace yqlora::builtin {

// Ресурсы, встроенные в движок. ID у них постоянные и маленькие,
// ID ресурсов проекта начинаются с kFirstProjectId и с ними не пересекаются.
// Сами меши - в BuiltinMeshes.hpp, шейдеры - в BuiltinShaders.hpp,
// материалы - в BuiltinResourceTable.cpp.
inline constexpr ResourceId kCubeMesh = 1;
inline constexpr ResourceId kSphereMesh = 2;
inline constexpr ResourceId kPlaneMesh = 3;
inline constexpr ResourceId kCapsuleMesh = 4;
inline constexpr ResourceId kCylinderMesh = 5;

// Освещённый шейдер PBR, с которого начинается каждый новый материал.
inline constexpr ResourceId kStandardShader = 0x101;
// Пурпурный: им рисуется материал, чей шейдер не компилируется.
inline constexpr ResourceId kErrorShader = 0x102;
// Шейдеры неба: атмосфера окружения с диском солнца, равнопромежуточная
// панорама, три цвета по высоте.
inline constexpr ResourceId kPhysicalSkyShader = 0x103;
inline constexpr ResourceId kPanoramicSkyShader = 0x104;
inline constexpr ResourceId kGradientSkyShader = 0x105;

// Материал пустого слота: шейдер Standard, белый.
inline constexpr ResourceId kDefaultMaterial = 0x201;
// Шейдер Error: им рисуется материал, чей шейдер не компилируется или пропал.
inline constexpr ResourceId kErrorMaterial = 0x202;
// Небо нового окружения: шейдер Physical со значениями по умолчанию.
inline constexpr ResourceId kDefaultSkyMaterial = 0x203;
// Небо из трёх цветов по высоте (шейдер Gradient); через него сцену освещают
// источники окружающего света Gradient и Color.
inline constexpr ResourceId kGradientSkyMaterial = 0x204;

inline constexpr ResourceId kFirstProjectId = 1024;

inline bool isBuiltin(ResourceId id)
{
    return id != 0 && id < kFirstProjectId;
}

} // namespace yqlora::builtin
