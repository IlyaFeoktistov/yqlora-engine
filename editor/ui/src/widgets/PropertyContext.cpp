#include "yqlora/editor/ui/widgets/PropertyContext.hpp"

#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ui/core/EditorHost.hpp"
#include "yqlora/scene/LayerSettings.hpp"

namespace yqlora::editor {

PropertyEditorContext propertyEditorContext(const EditorContext& context)
{
    EditorSession& session = context.session;
    const auto& project = session.project();
    const LayerSettings* layers = project ? static_cast<const LayerSettings*>(
                                                project->settings().find(typeNameOf<LayerSettings>())
                                            )
                                          : nullptr;

    return {
        .colors = context.theme.colors(),
        .metrics = context.theme.metrics(),
        .scene = session.scene(),
        .registry = session.registry(),
        .input = context.host.input(),
        .resources = project ? &project->resources() : nullptr,
        .cache = session.scene().services().resources(),
        .renderer = context.host.renderer(),
        .layerNames = layers ? std::span<const std::string>(layers->names()) : std::span<const std::string>(),
        .editLayers =
            [open = context.openProjectSettings] {
                open(typeNameOf<LayerSettings>());
            },
        .pings = &session.pings(),
    };
}

} // namespace yqlora::editor
