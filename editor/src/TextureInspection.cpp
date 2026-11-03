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

    return TextureInspection{texture, textureSamplingFromJson(importSettingsOf(session, id))};
}

bool applyTextureImport(EditorSession& session, ResourceId id, const TextureSampling& settings)
{
    return applyImportSettings(session, id, toJson(settings));
}

} // namespace yqlora::editor
