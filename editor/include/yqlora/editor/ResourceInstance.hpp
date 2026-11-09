#pragma once

#include "yqlora/resource/ResourceId.hpp"

#include <memory>

namespace yqlora {
class Node;
class ResourceCache;
class ResourceDatabase;
} // namespace yqlora

namespace yqlora::editor {

// Ветка сцены из ресурса проекта, ещё вне сцены:
// - модель (ModelData) - Node3D с именем файла, под ним узлы модели с их
//   локальными трансформами и MeshRenderer у узлов с мешем; один меш у
//   нескольких узлов - одна ссылка на один ресурс;
// - меш - Node3D с MeshRenderer и именем меша.
// Слоты MeshRenderer меша из модели заполнены материалами её файла.
// nullptr, если ресурс не модель и не меш или не загрузился.
std::unique_ptr<Node> instantiateResource(ResourceId id, ResourceCache& cache, const ResourceDatabase& resources);

} // namespace yqlora::editor
