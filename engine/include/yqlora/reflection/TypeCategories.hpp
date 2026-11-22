#pragma once

#include <string_view>

// Полки окна выбора типа (BasicTypeBuilder::category) и страницы руководства
// (BasicTypeBuilder::guide) у типов движка. Часть пути полки - ключ перевода
// category.<часть>; страница - файл docs/guide/<страница>.md.
namespace yqlora::categories {

inline constexpr char kSeparator = '/';

inline constexpr std::string_view kLogic = "Logic";
inline constexpr std::string_view kRendering = "Rendering";
inline constexpr std::string_view kPhysics = "Physics";
inline constexpr std::string_view kColliders = "Physics/Colliders";
inline constexpr std::string_view kJoints = "Physics/Joints";
// Скрипты игры: под ней - полки по папкам их исходников в проекте.
inline constexpr std::string_view kScripts = "Scripts";

} // namespace yqlora::categories

namespace yqlora::guides {

inline constexpr std::string_view kFirstScript = "first-script";
inline constexpr std::string_view kImportModel = "import-model";
inline constexpr std::string_view kPhysics = "physics";
inline constexpr std::string_view kPrefabs = "prefabs";
inline constexpr std::string_view kShaderMaterialSky = "shader-material-sky";

} // namespace yqlora::guides
