#pragma once

#include "yqlora/resource/ResourceKinds.hpp"

#include <string_view>

namespace yqlora {

// Префаб: ветка узлов, сохранённая один раз и поставленная много раз (см.
// docs/architecture/prefabs.md). Здесь только вид ресурса для полей
// Ref<Prefab>: такое поле берёт файлы префабов и модели (kinds::fitsKind).
struct Prefab {
    static constexpr std::string_view kResourceKind = kinds::kPrefab;
};

inline constexpr std::string_view kPrefabFileExtension = ".yprefab";

} // namespace yqlora
