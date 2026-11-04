#include "yqlora/render/ResourceUses.hpp"

#include "yqlora/material/MaterialData.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceKinds.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace yqlora {

namespace {

// Назначение материала - вид его шейдера.
constexpr std::pair<std::string_view, ShaderKind> kMaterialUses[] = {
    {uses::kSkyMaterial, ShaderKind::Sky},
    {uses::kSurfaceMaterial, ShaderKind::Surface},
};

} // namespace

bool fitsResourceUse(std::string_view use, ResourceId id, ResourceCache& cache)
{
    if (use.empty()) {
        return true;
    }
    const auto found = std::ranges::find(kMaterialUses, use, &std::pair<std::string_view, ShaderKind>::first);
    if (found == std::end(kMaterialUses)) {
        return false;
    }
    const MaterialData* material = cache.find<MaterialData>(id);
    if (!material) {
        return false;
    }
    const ShaderData* shader = cache.find<ShaderData>(material->shader);

    return !shader || shader->kind == found->second;
}

} // namespace yqlora
