#include "yqlora/physics/ConvexCollider.hpp"

namespace yqlora {

void ConvexCollider::setMesh(ResourceRef mesh)
{
    m_mesh = mesh;
    changed();
}

ColliderShape ConvexCollider::shape() const
{
    return MeshShape{.mesh = m_mesh.id, .convex = true};
}

} // namespace yqlora
