#pragma once

#include "yqlora/editor/ui/core/EditorPanel.hpp"
#include "yqlora/editor/ui/panels/ResourceInspector.hpp"

#include "yqlora/scene/NodeId.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace yqlora {
class Property;
} // namespace yqlora

namespace yqlora::editor {

// Свойства выбранного узла и его компоненты. Компоненты можно добавлять,
// менять и удалять, кроме встроенного Transform. Если в "Файлах" выбран
// ресурс, показывает его (ResourceInspector).
class InspectorPanel final : public EditorPanel {
public:
    const char* id() const override
    {
        return "Inspector";
    }

    const char* titleKey() const override
    {
        return "editor.window.inspector";
    }

    void draw(EditorContext& context) override;

    // Действие с экземпляром префаба, выбранное в этом кадре. Выполняется
    // после кадра панели: экземпляр перестраивается, а его узлы и
    // компоненты в кадре ещё рисуются.
    struct PrefabAction {
        enum class Kind {
            RevertProperty,
            ApplyProperty,
            RevertComponent,
            ApplyComponent,
            RevertInstance,
            ApplyInstance,
            RemoveUnused,
            Unpack,
            OpenPrefab,
        };

        Kind kind = Kind::RevertProperty;
        NodeId node = 0;
        // Компонент узла по месту в списке; nullopt - свойство самого узла.
        std::optional<std::size_t> component;
        const Property* property = nullptr;
    };

private:
    void runPrefabAction(EditorContext& context);

    ResourceInspector m_resource;
    std::optional<PrefabAction> m_prefabAction;
};

} // namespace yqlora::editor
