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

} // namespace yqlora::kinds
