#include "yqlora/resource/ResourceCache.hpp"

#include "yqlora/core/JsonFile.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/core/Text.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/project/Project.hpp"
#include "yqlora/resource/BuiltinResourceTable.hpp"
#include "yqlora/resource/BuiltinResources.hpp"
#include "yqlora/resource/ResourceLoader.hpp"

#include <algorithm>

namespace yqlora {

namespace fs = std::filesystem;

namespace {

std::string lowerExtension(const fs::path& file)
{
    return toLowerAscii(toUtf8(file.extension()));
}

} // namespace

ResourceCache::ResourceCache() = default;

ResourceCache::~ResourceCache() = default;

void ResourceCache::addLoader(std::unique_ptr<ResourceLoader> loader)
{
    m_loaders.push_back(std::move(loader));
}

void ResourceCache::setProject(Project* project)
{
    m_project = project;
    m_files.clear();
    std::erase_if(m_entries, [](const auto& entry) {
        return !builtin::isBuiltin(entry.first);
    });
}

std::string_view ResourceCache::kindOf(const fs::path& file) const
{
    const ResourceLoader* loader = loaderFor(file);

    return loader ? loader->kind() : std::string_view();
}

std::span<const ResourceId> ResourceCache::resourcesOf(ResourceId file)
{
    const auto path = m_project ? m_project->resources().path(file) : std::nullopt;
    if (!path) {
        return {};
    }
    if (!m_files.contains(*path)) {
        loadFile(*path);
    }

    return m_files.at(*path).ids;
}

std::span<const std::string_view> ResourceCache::nestedKindsOf(const fs::path& file) const
{
    const ResourceLoader* loader = loaderFor(file);

    return loader ? loader->nestedKinds() : std::span<const std::string_view>();
}

const ResourceStatus* ResourceCache::status(ResourceId id) const
{
    const auto it = m_entries.find(id);

    return it != m_entries.end() ? &it->second.status : nullptr;
}

bool ResourceCache::reload(ResourceId id)
{
    const auto path = m_project ? m_project->resources().path(id) : std::nullopt;
    if (!path) {
        return false;
    }
    loadFile(*path);

    return true;
}

std::size_t ResourceCache::reloadChanged()
{
    std::size_t reloaded = 0;
    std::vector<fs::path> forgotten;
    for (const auto& [file, entry] : m_files) {
        // Файл переименовали или удалили через редактор: его ресурсы найдутся
        // по новому пути при следующем обращении.
        if (!m_project || m_project->resources().id(file) == 0) {
            forgotten.push_back(file);
            continue;
        }
        const bool changed = stampOf(m_project->directory() / file) != entry.stamp
                             || std::ranges::any_of(entry.dependencies, [](const auto& dependency) {
                                    return stampOf(dependency.first) != dependency.second;
                                });
        if (changed) {
            forgotten.push_back(file);
            ++reloaded;
        }
    }
    for (const fs::path& file : forgotten) {
        const bool known = m_project && m_project->resources().id(file) != 0;
        if (known) {
            loadFile(file);
            continue;
        }
        for (const ResourceId id : m_files[file].ids) {
            m_entries.erase(id);
        }
        m_files.erase(file);
    }
    // Ресурсы, которых не было в проекте: их могли добавить.
    std::erase_if(m_entries, [this](const auto& entry) {
        const bool inFile = std::ranges::any_of(m_files, [&](const auto& file) {
            return std::ranges::find(file.second.ids, entry.first) != file.second.ids.end();
        });

        return !inFile && !builtin::isBuiltin(entry.first) && entry.second.status.state == ResourceState::Missing;
    });

    return reloaded;
}

ResourceCache::Found ResourceCache::getData(ResourceId id, std::string_view kind)
{
    if (id == 0) {
        return {nullptr, 0};
    }
    const Entry* found = lookup(id);
    if (!found) {
        log::warn("Resource {:016x} is not in the project", id);
        setEntry(id, {{}, nullptr, {ResourceState::Missing, 0, "The resource is not in the project"}});
        found = &m_entries.at(id);
    }
    const Entry& entry = *found;
    const bool fits = entry.data && entry.kind == kind;

    return {fits ? entry.data.get() : nullptr, entry.status.revision};
}

bool ResourceCache::replaceData(ResourceId id, std::string_view kind, std::shared_ptr<const void> data)
{
    const auto it = m_entries.find(id);
    if (builtin::isBuiltin(id) || it == m_entries.end() || !it->second.data || it->second.kind != kind) {
        return false;
    }
    setEntry(id, {it->second.kind, std::move(data), {ResourceState::Ready, 0, {}}});

    return true;
}

std::string_view ResourceCache::loadedKind(ResourceId id)
{
    const Entry* entry = id != 0 ? lookup(id) : nullptr;

    return entry && entry->data ? entry->kind : std::string_view();
}

const ResourceCache::Entry* ResourceCache::lookup(ResourceId id)
{
    auto it = m_entries.find(id);
    if (it == m_entries.end() && builtin::isBuiltin(id) && makeBuiltin(id)) {
        it = m_entries.find(id);
    }
    if (it == m_entries.end()) {
        // Уже прочитанный файл, в котором ресурса нет, второй раз не читается:
        // изменения файла подхватит reloadChanged.
        const auto path = m_project ? m_project->resources().path(id) : std::nullopt;
        if (path && !m_files.contains(*path)) {
            loadFile(*path);
        }
        it = m_entries.find(id);
    }

    return it != m_entries.end() ? &it->second : nullptr;
}

ResourceCache::FileStamp ResourceCache::stampOf(const fs::path& full)
{
    FileStamp stamp;
    std::error_code ec;
    stamp.existed = fs::is_regular_file(full, ec);
    if (stamp.existed) {
        stamp.time = fs::last_write_time(full, ec);
    }

    return stamp;
}

bool ResourceCache::makeBuiltin(ResourceId id)
{
    const BuiltinResourceEntry* found = findBuiltinResource(id);
    if (!found) {
        return false;
    }
    setEntry(id, {found->kind, found->make(), {ResourceState::Ready, 0, {}}});

    return true;
}

void ResourceCache::updateImport(ResourceId file, const nlohmann::json& update)
{
    if (update.empty()) {
        return;
    }
    ResourceDatabase& database = m_project->resources();
    auto import = nlohmann::json::parse(database.importSettings(file), nullptr, false);
    if (!import.is_object()) {
        import = nlohmann::json::object();
    }
    nlohmann::json merged = import;
    merged.update(update);
    if (merged != import) {
        database.setImportSettings(file, merged.dump());
    }
}

std::vector<ResourceId> ResourceCache::nestedIds(ResourceId file, std::span<const std::string> keys)
{
    ResourceDatabase& database = m_project->resources();
    if (m_metaPolicy == MetaPolicy::Import) {
        return database.assignSubresources(file, keys);
    }
    const auto& known = database.subresources(file);
    std::vector<ResourceId> ids;
    ids.reserve(keys.size());
    for (const std::string& key : keys) {
        const auto found = known.find(key);
        ids.push_back(found != known.end() ? found->second : 0);
    }

    return ids;
}

void ResourceCache::loadFile(const fs::path& file)
{
    ResourceDatabase& database = m_project->resources();
    const fs::path full = m_project->directory() / file;
    const ResourceId own = database.id(file);
    FileEntry loaded{.stamp = stampOf(full)};

    // Новые записи собираются целиком, прежние заменяются только потом;
    // order - порядок ресурсов в файле.
    std::map<ResourceId, Entry> fresh;
    std::vector<ResourceId> order;
    const auto put = [&](ResourceId id, Entry entry) {
        if (!fresh.contains(id)) {
            order.push_back(id);
        }
        fresh[id] = std::move(entry);
    };
    const auto fail = [&](ResourceState state, std::string message) {
        put(own, {{}, nullptr, {state, 0, std::move(message)}});
    };
    const ResourceLoader* loader = loaderFor(file);
    LoadResult result;
    if (loaded.stamp.existed && loader) {
        result = loader->load(request(own, file));
        for (const fs::path& dependency : result.dependencies) {
            loaded.dependencies.emplace_back(dependency, stampOf(dependency));
        }
        if (m_metaPolicy == MetaPolicy::Import) {
            updateImport(own, result.importUpdate);
        }
    }
    if (!loaded.stamp.existed) {
        fail(ResourceState::Missing, "The file is missing");
    } else if (!loader) {
        fail(ResourceState::Error, "No loader for files " + lowerExtension(file));
    } else if (!result.error.empty()) {
        fail(ResourceState::Error, std::move(result.error));
    } else {
        std::vector<std::string> keys;
        for (const LoadedResource& resource : result.resources) {
            if (!resource.key.empty()) {
                keys.push_back(resource.key);
            }
        }
        const std::vector<ResourceId> nested = nestedIds(own, keys);
        std::map<std::string_view, ResourceId> byKey;
        for (std::size_t i = 0; i < keys.size(); ++i) {
            byKey.emplace(keys[i], nested[i]);
        }
        auto nextNested = nested.begin();
        std::size_t notImported = 0;
        for (LoadedResource& resource : result.resources) {
            const ResourceId id = resource.key.empty() ? own : *nextNested++;
            if (id == 0) {
                ++notImported;
                continue;
            }
            for (const NestedLink& link : resource.links) {
                if (const auto found = byKey.find(link.key); found != byKey.end() && found->second != 0) {
                    link.resolve(found->second);
                }
            }
            put(id, {resource.kind, std::move(resource.data), {ResourceState::Ready, 0, {}}});
        }
        if (notImported != 0) {
            log::warn(
                "Resource '{}': {} nested resources have no id in its .meta and are skipped; open the project in "
                "the editor to import them",
                toGenericUtf8(file),
                notImported
            );
        }
        if (!fresh.contains(own)) {
            fail(ResourceState::Error, "The file holds no resource of its own");
        }
    }

    // Ресурсы, которые были в файле раньше, а теперь их нет.
    if (const auto previous = m_files.find(file); previous != m_files.end()) {
        for (const ResourceId id : previous->second.ids) {
            if (!fresh.contains(id)) {
                put(id, {{}, nullptr, {ResourceState::Missing, 0, "The resource is not in the file any more"}});
            }
        }
    }
    for (const ResourceId id : order) {
        Entry& entry = fresh.at(id);
        if (entry.status.state == ResourceState::Missing) {
            log::warn("Resource '{}': {}", toGenericUtf8(file), entry.status.message);
        } else if (entry.status.state == ResourceState::Error) {
            log::error("Resource '{}' is not loaded: {}", toGenericUtf8(file), entry.status.message);
        }
        loaded.ids.push_back(id);
        setEntry(id, std::move(entry));
    }
    m_files[file] = std::move(loaded);
}

LoadRequest ResourceCache::request(ResourceId file, const fs::path& relative) const
{
    LoadRequest request{.file = m_project->directory() / relative, .relative = relative};
    const auto import = nlohmann::json::parse(m_project->resources().importSettings(file), nullptr, false);
    if (import.is_object()) {
        request.import = import;
    }

    return request;
}

const ResourceLoader* ResourceCache::loaderFor(const fs::path& file) const
{
    const std::string extension = lowerExtension(file);
    for (const auto& loader : m_loaders) {
        if (std::ranges::find(loader->extensions(), extension) != loader->extensions().end()) {
            return loader.get();
        }
    }

    return nullptr;
}

void ResourceCache::setEntry(ResourceId id, Entry entry)
{
    entry.status.revision = ++m_lastRevision;
    m_entries[id] = std::move(entry);
}

} // namespace yqlora
