#include "yqlora/editor/TextureInspection.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ResourceInspection.hpp"
#include "yqlora/import/TextureImport.hpp"

namespace yqlora::editor {

std::optional<TextureInspection> inspectTexture(EditorSession& session, ResourceId id)
{
    const LoadedData<TextureData> texture = session.resources().get<TextureData>(id);
    if (!texture) {
        return std::nullopt;
    }

    if (!isResourceFile(session, id)) {
        return TextureInspection{.texture = texture, .import = texture.data->sampling, .editable = false};
    }

    return TextureInspection{
        .texture = texture,
        .import = textureSamplingFromJson(importSettingsOf(session, id)),
        .editable = true,
    };
}

bool applyTextureImport(EditorSession& session, ResourceId id, const TextureSampling& settings)
{
    return applyImportSettings(session, id, toJson(settings));
}

} // namespace yqlora::editor
