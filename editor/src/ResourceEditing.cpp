#include "yqlora/editor/ResourceEditing.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ResourceInspection.hpp"
#include "yqlora/material/MaterialFile.hpp"
#include "yqlora/physics/PhysicsMaterialFile.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

namespace yqlora::editor {

namespace {

// Виды, которые правятся в инспекторе, и запись их файла.
struct ResourceWriter {
    std::string_view kind;
    bool (*write)(ResourceCache& cache, ResourceId id, const std::filesystem::path& file);
};

template <typename T, bool (*Write)(const std::filesystem::path&, const T&)>
bool writeAs(ResourceCache& cache, ResourceId id, const std::filesystem::path& file)
{
    const T* data = cache.find<T>(id);

    return data && Write(file, *data);
}

const ResourceWriter kWriters[] = {
    {MaterialData::kResourceKind, writeAs<MaterialData, writeMaterialFile>},
    {PhysicsMaterialData::kResourceKind, writeAs<PhysicsMaterialData, writePhysicsMaterialFile>},
};

const ResourceWriter* writerOf(std::string_view kind)
{
    const auto found = std::ranges::find(kWriters, kind, &ResourceWriter::kind);

    return found != std::end(kWriters) ? &*found : nullptr;
}

} // namespace

ResourceCache& ResourceEditing::cache() const
{
    return m_session.resources();
}

bool ResourceEditing::editable(ResourceId id) const
{
    return writerOf(cache().loadedKind(id)) && isResourceFile(m_session, id);
}

void ResourceEditing::beforeChange(ResourceId id)
{
    if (m_pending != 0 && m_pending != id) {
        finish();
    }
}

void ResourceEditing::afterChange(ResourceId id)
{
    if (m_pending == 0) {
        m_sinceWrite = 0.0f;
    }
    m_pending = id;
}

bool ResourceEditing::finish()
{
    return m_pending == 0 || write();
}

void ResourceEditing::update(float deltaSeconds)
{
    if (m_pending == 0) {
        return;
    }
    m_sinceWrite += deltaSeconds;
    if (m_sinceWrite >= 1.0f / kWritesPerSecond) {
        write();
    }
}

bool ResourceEditing::write()
{
    const ResourceId id = std::exchange(m_pending, 0);
    m_sinceWrite = 0.0f;
    const auto& project = m_session.project();
    const auto path = project ? project->resources().path(id) : std::nullopt;
    const ResourceWriter* writer = writerOf(cache().loadedKind(id));
    if (!path || !writer || !writer->write(cache(), id, project->directory() / *path)) {
        log::error("Resource '{}' is not written", path ? toGenericUtf8(*path) : std::to_string(id));

        return false;
    }

    return true;
}

} // namespace yqlora::editor
