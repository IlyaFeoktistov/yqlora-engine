#include "yqlora/render/MeshRenderer.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {

MaterialOverrides& MeshRenderer::overrides(std::size_t slot)
{
    if (slot >= m_overrides.size()) {
        m_overrides.resize(slot + 1);
    }

    return m_overrides[slot];
}

std::vector<std::string> MeshRenderer::slotNames() const
{
    const Scene* scene = owner() ? owner()->scene() : nullptr;
    const MeshData* mesh = scene ? scene->services().resources().find<MeshData>(m_mesh.id) : nullptr;
    std::vector<std::string> names;
    if (mesh) {
        for (const SubMesh& submesh : mesh->submeshes) {
            names.push_back(submesh.sourceMaterialName);
        }
    }

    return names;
}

const MaterialOverrides* MeshRenderer::findOverrides(std::size_t slot) const
{
    return slot < m_overrides.size() && !m_overrides[slot].empty() ? &m_overrides[slot] : nullptr;
}

} // namespace yqlora
