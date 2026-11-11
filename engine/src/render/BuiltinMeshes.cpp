#include "yqlora/render/BuiltinMeshes.hpp"

#include "yqlora/render/PrimitiveMeshes.hpp"
#include "yqlora/resource/BuiltinResources.hpp"

namespace yqlora {

std::span<const BuiltinMesh> builtinMeshes()
{
    static const BuiltinMesh meshes[] = {
        {builtin::kCubeMesh, "cube", &makeCube},
        {builtin::kSphereMesh, "sphere", &makeSphere},
        {builtin::kPlaneMesh, "plane", &makePlane},
        {builtin::kCapsuleMesh, "capsule", &makeCapsule},
        {builtin::kCylinderMesh, "cylinder", &makeCylinder},
    };

    return meshes;
}

} // namespace yqlora
