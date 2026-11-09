#pragma once

#include <string_view>

// Виды ресурсов: по ним поле ссылки знает, что ему подходит
// (PropertyAttributes::resourceKind), а загрузчик - что он даёт. Один список
// для движка, редактора и скриптов игры: типы данных берут вид отсюда
// (MaterialData::kResourceKind == kinds::kMaterial).
namespace yqlora::kinds {

inline constexpr std::string_view kMesh = "mesh";
inline constexpr std::string_view kModel = "model";
inline constexpr std::string_view kTexture = "texture";
inline constexpr std::string_view kShader = "shader";
inline constexpr std::string_view kMaterial = "material";
inline constexpr std::string_view kScene = "scene";
inline constexpr std::string_view kPrefab = "prefab";

// Ресурс вида kind годится полю вида fieldKind: тот же вид, а полю префаба -
// ещё и модель (модель - префаб, который только читается из файла).
constexpr bool fitsKind(std::string_view kind, std::string_view fieldKind)
{
    return kind == fieldKind || (fieldKind == kPrefab && kind == kModel);
}

} // namespace yqlora::kinds

// Назначения: какие ресурсы одного вида годятся полю
// (PropertyAttributes::resourceUse). Пусто - любой ресурс вида.
namespace yqlora::uses {

// Материал с шейдером неба: небо окружения.
inline constexpr std::string_view kSkyMaterial = "skyMaterial";
// Материал с шейдером поверхности: слоты меша.
inline constexpr std::string_view kSurfaceMaterial = "surfaceMaterial";

} // namespace yqlora::uses
