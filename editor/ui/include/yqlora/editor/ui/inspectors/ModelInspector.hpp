#pragma once

#include "yqlora/editor/ui/inspectors/ResourceKindInspector.hpp"

#include "yqlora/editor/ModelInspection.hpp"

#include <optional>

namespace yqlora::editor {

// Модель: число узлов, меши (их тянут в сцену) и масштаб импорта.
class ModelInspector final : public ResourceKindInspector {
public:
    void reset() override;
    void draw(EditorContext& context, ResourceId id) override;

private:
    // Правка настроек импорта; nullopt - ещё не начата.
    std::optional<ModelImport> m_import;
};

} // namespace yqlora::editor
