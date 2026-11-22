#include "yqlora/editor/ResourceInspection.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ResourceChoices.hpp"
#include "yqlora/resource/BuiltinResourceTable.hpp"
#include "yqlora/resource/ResourceLoader.hpp"

namespace yqlora::editor {

std::optional<ResourceInspection> inspectResource(EditorSession& session, ResourceId id)
{
    const auto& project = session.project();
    if (!project) {
        return std::nullopt;
    }
    ResourceCache& cache = session.resources();
    if (const BuiltinResourceEntry* builtin = findBuiltinResource(id)) {
        return ResourceInspection{
            .id = id,
            .name = std::string(builtin->name),
            .path = {},
            .nestedKey = {},
            .kind = builtin->kind,
            .status = std::nullopt,
        };
    }
    const auto path = project->resources().path(id);
    if (!path) {
        return std::nullopt;
    }
    const std::string_view key = project->resources().key(id);
    const std::string_view nested = subresourceLocalKey(key);
    ResourceInspection inspection{
        .id = id,
        .name = nested.empty() ? toUtf8(path->filename()) : std::string(nested),
        .path = *path,
        .nestedKey = std::string(subresourceLocalKey(key)),
        .kind = key.empty() ? cache.kindOf(*path) : cache.loadedKind(id),
        .status = std::nullopt,
    };
    if (cache.kindOf(*path).empty()) {
        return inspection;
    }
    cache.loadedKind(id);
    if (const ResourceStatus* status = cache.status(id)) {
        inspection.status = *status;
    }

    return inspection;
}

bool isResourceFile(const EditorSession& session, ResourceId id)
{
    const auto& project = session.project();

    return project && project->resources().path(id) && project->resources().key(id).empty();
}

nlohmann::json importSettingsOf(EditorSession& session, ResourceId id)
{
    const auto& project = session.project();
    if (!project) {
        return nlohmann::json::object();
    }
    nlohmann::json json = nlohmann::json::parse(project->resources().importSettings(id), nullptr, false);

    return json.is_object() ? json : nlohmann::json::object();
}

bool applyImportSettings(EditorSession& session, ResourceId id, const nlohmann::json& settings)
{
    auto& project = session.project();
    if (!project || !project->resources().setImportSettings(id, settings.dump())) {
        return false;
    }

    return session.resources().reload(id);
}

} // namespace yqlora::editor
