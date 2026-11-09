#include "yqlora/editor/MaterialEditing.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ResourceInspection.hpp"
#include "yqlora/material/MaterialFile.hpp"
#include "yqlora/resource/ResourceCache.hpp"

namespace yqlora::editor {

const MaterialData* MaterialEditing::material(ResourceId id) const
{
    return m_session.resources().find<MaterialData>(id);
}

bool MaterialEditing::editable(ResourceId id) const
{
    return material(id) && isResourceFile(m_session, id);
}

bool MaterialEditing::change(ResourceId id, const std::function<void(MaterialData&)>& edit)
{
    const MaterialData* current = material(id);
    if (!current || !editable(id)) {
        return false;
    }
    // Другой материал: правка прежнего дописывается в его файл.
    if (m_pending != 0 && m_pending != id) {
        finish();
    }
    MaterialData changed = *current;
    edit(changed);
    if (!m_session.resources().replace(id, std::move(changed))) {
        return false;
    }
    if (m_pending == 0) {
        m_sinceWrite = 0.0f;
    }
    m_pending = id;

    return true;
}

bool MaterialEditing::finish()
{
    return m_pending == 0 || write();
}

void MaterialEditing::update(float deltaSeconds)
{
    if (m_pending == 0) {
        return;
    }
    m_sinceWrite += deltaSeconds;
    if (m_sinceWrite >= 1.0f / kWritesPerSecond) {
        write();
    }
}

bool MaterialEditing::write()
{
    const ResourceId id = std::exchange(m_pending, 0);
    m_sinceWrite = 0.0f;
    const auto& project = m_session.project();
    const auto path = project ? project->resources().path(id) : std::nullopt;
    const MaterialData* data = material(id);
    if (!path || !data || !writeMaterialFile(project->directory() / *path, *data)) {
        log::error("Material '{}' is not written", path ? toGenericUtf8(*path) : std::to_string(id));

        return false;
    }

    return true;
}

} // namespace yqlora::editor
