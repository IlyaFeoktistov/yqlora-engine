#include "yqlora/physics/MeshCollider.hpp"

namespace yqlora {

void MeshCollider::setMesh(ResourceRef mesh)
{
    m_mesh = mesh;
    changed();
}

ColliderShape MeshCollider::shape() const
{
    return MeshShape{.mesh = m_mesh.id, .convex = false};
}

} // namespace yqlora
