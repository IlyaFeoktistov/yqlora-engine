#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <nlohmann/json.hpp>

#include <optional>

namespace yqlora {

class ResourceCache;
class ResourceDatabase;
class TypeRegistry;

// Модель как префаб: её дерево в формате ветки сцены (SceneBranches).
// - Модель (ModelData): Node3D с именем файла, под ним узлы модели с их
//   локальными трансформами и MeshRenderer у узлов с мешем; один меш у
//   нескольких узлов - одна ссылка на один ресурс.
// - Меш: Node3D с MeshRenderer и именем меша.
// Слоты MeshRenderer меша из модели заполнены материалами её файла.
//
// ID узлов выведены из ключей узлов в файле ("node:LeftDoor", у второго с тем
// же именем - "node:LeftDoor#1"), ID компонентов - из ID узла и имени типа:
// перевыгрузка с теми же именами даёт те же ID, и ссылки на них живут.
// nullopt, если resource - не модель и не меш или не загрузился.
YQLORA_API std::optional<nlohmann::json> modelBranch(
    ResourceId resource,
    ResourceCache& cache,
    const ResourceDatabase* resources,
    const TypeRegistry& registry
);

} // namespace yqlora
