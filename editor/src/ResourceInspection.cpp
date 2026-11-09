#include "yqlora/editor/ResourceInspection.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ResourceChoices.hpp"
#include "yqlora/resource/ResourceLoader.hpp"

namespace yqlora::editor {

std::optional<ResourceInspection> inspectResource(EditorSession& session, ResourceId id)
{
    const auto& project = session.project();
    const auto path = project ? project->resources().path(id) : std::nullopt;
    if (!path) {
        return std::nullopt;
    }
    ResourceCache& cache = session.resources();
    const std::string_view key = project->resources().key(id);
    ResourceInspection inspection{
        .id = id,
        .path = *path,
        .nestedKey = std::string(subresourceLocalKey(key)),
        .kind = key.empty() ? fileKind(*path, cache) : cache.loadedKind(id),
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
