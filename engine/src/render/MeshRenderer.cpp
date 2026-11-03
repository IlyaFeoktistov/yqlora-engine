#include "yqlora/render/MeshRenderer.hpp"

namespace yqlora {

MaterialOverrides& MeshRenderer::overrides(std::size_t slot)
{
    if (slot >= m_overrides.size()) {
        m_overrides.resize(slot + 1);
    }

    return m_overrides[slot];
}

const MaterialOverrides* MeshRenderer::findOverrides(std::size_t slot) const
{
    return slot < m_overrides.size() && !m_overrides[slot].empty() ? &m_overrides[slot] : nullptr;
}

} // namespace yqlora
