#pragma once

#include "yqlora/resource/ResourceId.hpp"

#include <memory>

namespace yqlora {
class Node;
class ResourceCache;
class ResourceDatabase;
class TypeRegistry;
} // namespace yqlora

namespace yqlora::editor {

// Ветка сцены из модели или меша проекта, ещё вне сцены: дерево модели как
// префаба (modelBranch, там же - что в нём), с новыми ID узлов и
// компонентов, чтобы два броска одной модели не совпадали. nullptr, если
// ресурс не модель и не меш или не загрузился.
std::unique_ptr<Node> instantiateResource(
    ResourceId id,
    ResourceCache& cache,
    const ResourceDatabase& resources,
    const TypeRegistry& registry
);

} // namespace yqlora::editor
