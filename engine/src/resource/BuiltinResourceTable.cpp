#include "yqlora/resource/BuiltinResourceTable.hpp"

#include "yqlora/render/BuiltinMeshes.hpp"
#include "yqlora/shader/BuiltinShaders.hpp"

#include <vector>

namespace yqlora {

std::span<const BuiltinResourceEntry> builtinResourceTable()
{
    static const std::vector<BuiltinResourceEntry> table = [] {
        std::vector<BuiltinResourceEntry> entries;
        for (const BuiltinMesh& mesh : builtinMeshes()) {
            entries.push_back({mesh.id, MeshData::kResourceKind, [make = mesh.make] {
                                   return std::shared_ptr<const void>(std::make_shared<const MeshData>(make()));
                               }});
        }
        for (const BuiltinShader& shader : builtinShaders()) {
            entries.push_back(
                {shader.id, ShaderData::kResourceKind, [&shader] {
                     return std::shared_ptr<const void>(std::make_shared<const ShaderData>(makeBuiltinShader(shader)));
                 }}
            );
        }

        return entries;
    }();

    return table;
}

} // namespace yqlora
