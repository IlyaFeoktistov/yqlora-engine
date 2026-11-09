#pragma once

#include "yqlora/render/TextureData.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <optional>

namespace yqlora::editor {

class EditorSession;

// Что инспектор текстуры показывает: данные для превью и размера и
// настройки импорта, как они записаны в .meta. У текстуры модели настройки -
// из её файла (сэмплер glTF), и они только читаются.
struct TextureInspection {
    LoadedData<TextureData> texture;
    TextureSampling import;
    // Настройки импорта можно менять (isResourceFile).
    bool editable = false;
};

// nullopt, если id - не текстура проекта или текстура не загрузилась.
std::optional<TextureInspection> inspectTexture(EditorSession& session, ResourceId id);

// См. applyImportSettings в ResourceInspection.hpp.
bool applyTextureImport(EditorSession& session, ResourceId id, const TextureSampling& settings);

} // namespace yqlora::editor
