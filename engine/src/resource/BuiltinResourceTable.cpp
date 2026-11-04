#include "yqlora/resource/BuiltinResourceTable.hpp"

#include "yqlora/material/MaterialData.hpp"
#include "yqlora/render/BuiltinMeshes.hpp"
#include "yqlora/shader/BuiltinShaders.hpp"

#include <algorithm>
#include <vector>

namespace yqlora {

std::span<const BuiltinResourceEntry> builtinResourceTable()
{
    static const std::vector<BuiltinResourceEntry> table = [] {
        std::vector<BuiltinResourceEntry> entries;
        for (const BuiltinMesh& mesh : builtinMeshes()) {
            entries.push_back({mesh.id, MeshData::kResourceKind, mesh.name, [make = mesh.make] {
                                   return std::shared_ptr<const void>(std::make_shared<const MeshData>(make()));
                               }});
        }
        for (const BuiltinShader& shader : builtinShaders()) {
            entries.push_back(
                {shader.id, ShaderData::kResourceKind, shader.name, [&shader] {
                     return std::shared_ptr<const void>(std::make_shared<const ShaderData>(makeBuiltinShader(shader)));
                 }}
            );
        }

        // Материалы - значения по умолчанию своих шейдеров.
        const auto material = [](ResourceId shader) {
            return [shader] {
                return std::shared_ptr<const void>(
                    std::make_shared<const MaterialData>(MaterialData{.shader = shader})
                );
            };
        };
        entries.push_back(
            {builtin::kDefaultMaterial, MaterialData::kResourceKind, "Default", material(builtin::kStandardShader)}
        );
        entries.push_back(
            {builtin::kErrorMaterial, MaterialData::kResourceKind, "Error", material(builtin::kErrorShader)}
        );
        entries.push_back(
            {builtin::kDefaultSkyMaterial,
             MaterialData::kResourceKind,
             "Default Sky",
             material(builtin::kPhysicalSkyShader)}
        );
        entries.push_back(
            {builtin::kGradientSkyMaterial,
             MaterialData::kResourceKind,
             "Gradient Sky",
             material(builtin::kGradientSkyShader)}
        );

        return entries;
    }();

    return table;
}

const BuiltinResourceEntry* findBuiltinResource(ResourceId id)
{
    const auto table = builtinResourceTable();
    const auto found = std::ranges::find(table, id, &BuiltinResourceEntry::id);

    return found != table.end() ? &*found : nullptr;
}

} // namespace yqlora
