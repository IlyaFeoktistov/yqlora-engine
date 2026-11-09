#include "yqlora/editor/ResourceInstance.hpp"

#include "yqlora/import/ModelPrefab.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/SceneBranches.hpp"

namespace yqlora::editor {

std::unique_ptr<Node> instantiateResource(
    ResourceId id,
    ResourceCache& cache,
    const ResourceDatabase& resources,
    const TypeRegistry& registry
)
{
    const auto branch = modelBranch(id, cache, &resources, registry);

    return branch ? SceneBranches::read(registry, *branch, BranchIds::Fresh) : nullptr;
}

} // namespace yqlora::editor
