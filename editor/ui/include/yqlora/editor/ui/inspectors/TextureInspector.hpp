#pragma once

#include "yqlora/editor/ui/inspectors/ResourceKindInspector.hpp"

#include "yqlora/render/TextureData.hpp"

#include <optional>

namespace yqlora::editor {

// Текстура: размер, превью и настройки импорта (цвет, фильтр, повтор, мипмапы).
class TextureInspector final : public ResourceKindInspector {
public:
    void reset() override;
    void draw(EditorContext& context, ResourceId id) override;

private:
    // Правка настроек импорта; nullopt - ещё не начата.
    std::optional<TextureSampling> m_import;
};

} // namespace yqlora::editor
