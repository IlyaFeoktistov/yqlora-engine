#pragma once

#include "yqlora/render/TextureData.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <optional>

namespace yqlora::editor {

class EditorSession;

// Что инспектор текстуры показывает: данные для превью и размера и
// настройки импорта, как они записаны в .meta.
struct TextureInspection {
    LoadedData<TextureData> texture;
    TextureSampling import;
};

// nullopt, если id - не файл текстуры проекта или текстура не загрузилась.
std::optional<TextureInspection> inspectTexture(EditorSession& session, ResourceId id);

// См. applyImportSettings в ResourceInspection.hpp.
bool applyTextureImport(EditorSession& session, ResourceId id, const TextureSampling& settings);

} // namespace yqlora::editor
