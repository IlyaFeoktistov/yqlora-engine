#include "yqlora/editor/ui/style/ResourceIcons.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"

#include "yqlora/editor/ProjectFiles.hpp"
#include "yqlora/import/ModelData.hpp"
#include "yqlora/material/MaterialData.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/scene/Prefab.hpp"
#include "yqlora/scene/SceneSerializer.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <algorithm>
#include <utility>

namespace yqlora::editor {

namespace {

constexpr std::pair<std::string_view, const char*> kIcons[] = {
    {kFolderKind, icons::kFolder},
    {kCppScriptKind, icons::kCodeFile},
    {SceneSerializer::kResourceKind, icons::kScene},
    {Prefab::kResourceKind, icons::kPrefab},
    {ModelData::kResourceKind, icons::kModelFile},
    {MeshData::kResourceKind, icons::kMesh},
    {TextureData::kResourceKind, icons::kImageFile},
    {ShaderData::kResourceKind, icons::kShaderFile},
    {kSkyShaderKind, icons::kSkyShader},
    {MaterialData::kResourceKind, icons::kMaterialFile},
};

} // namespace

const char* resourceIcon(std::string_view kind)
{
    const auto found = std::ranges::find(kIcons, kind, &std::pair<std::string_view, const char*>::first);

    return found != std::end(kIcons) ? found->second : icons::kFile;
}

} // namespace yqlora::editor
