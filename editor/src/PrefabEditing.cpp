#include "yqlora/editor/PrefabEditing.hpp"

#include "yqlora/core/JsonId.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ProjectFiles.hpp"
#include "yqlora/editor/SceneCommands.hpp"
#include "yqlora/reflection/PropertyJson.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/PrefabBases.hpp"
#include "yqlora/scene/PrefabExpansion.hpp"
#include "yqlora/scene/PrefabInstance.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/SceneBranches.hpp"
#include "yqlora/scene/SceneFileKeys.hpp"
#include "yqlora/scene/SceneSerializer.hpp"

#include <algorithm>
#include <functional>
#include <string>

namespace yqlora::editor {

namespace {

using Json = nlohmann::json;
namespace keys = scene_keys;

using EntryFilter = std::function<bool(std::string_view kind, const Json& entry)>;

std::uint64_t idAt(const Json& object, const char* key)
{
    const auto it = object.find(key);

    return it != object.end() ? idFromJson(*it).value_or(0) : 0;
}

const Json& arrayAt(const Json& object, const char* key)
{
    static const Json kEmpty = Json::array();
    const auto it = object.find(key);

    return it != object.end() && it->is_array() ? *it : kEmpty;
}

// Переопределение - своё значение корня: имя или свойство его встроенного
// компонента (положение). Их не возвращают и не применяют к префабу.
bool rootOwn(
    const PrefabInstance& instance,
    NodeId rootSource,
    const TypeRegistry& registry,
    std::string_view kind,
    const Json& entry
)
{
    if (kind != keys::kOverrides || idAt(entry, keys::kNode) != rootSource) {
        return false;
    }
    const ComponentId component = idAt(entry, keys::kComponent);
    if (component == 0) {
        return entry.value(keys::kProperty, std::string()) == keys::kNameProperty;
    }
    for (const Json& candidate : arrayAt(*instance.base, keys::kComponents)) {
        if (idAt(candidate, keys::kId) == component) {
            const TypeInfo* type = registry.find(candidate.value(keys::kType, std::string()));

            return type && type->builtin();
        }
    }

    return false;
}

bool unusedEntry(const PrefabInstance& instance, std::string_view kind, const Json& entry)
{
    const Json& unused = arrayAt(instance.unused, std::string(kind).c_str());

    return std::ranges::find(unused, entry) != unused.end();
}

// Переопределение, которое возвращают и применяют: не своё значение корня
// и не без цели.
EntryFilter editable(const Node& root, const TypeRegistry& registry)
{
    const PrefabInstance& instance = *root.prefabInstance();
    const NodeId source = root.prefabLink().source;

    return [&instance, source, &registry](std::string_view kind, const Json& entry) {
        return !rootOwn(instance, source, registry, kind, entry) && !unusedEntry(instance, kind, entry);
    };
}

bool hasEntries(const Json& record)
{
    for (const char* kind :
         {keys::kOverrides, keys::kAddedComponents, keys::kRemovedComponents, keys::kAdded, keys::kRemoved}) {
        if (!arrayAt(record, kind).empty()) {
            return true;
        }
    }

    return false;
}

std::unique_ptr<Node> readBranch(EditorSession& session, const Json& json)
{
    const PrefabSources sources = PrefabSources::of(session.services());

    return SceneBranches::read(session.registry(), json, BranchIds::FromFile, &sources);
}

// Узел заменяется веткой fresh на том же месте.
Node* replaceNode(EditorSession& session, Node& node, std::unique_ptr<Node> fresh)
{
    if (!fresh) {
        return nullptr;
    }
    Node* result = fresh.get();
    Node* parent = node.parent();
    if (!parent) {
        session.scene().setRoot(std::move(fresh));
    } else {
        const auto children = parent->children();
        const auto it = std::ranges::find_if(children, [&](const auto& child) {
            return child.get() == &node;
        });
        const auto index = static_cast<std::size_t>(it - children.begin());
        parent->removeChild(node);
        parent->insertChild(index, std::move(fresh));
    }
    session.selection().keepNodes([&](NodeId id) {
        return session.findNode(id) != nullptr;
    });

    return result;
}

// Узел заменяется прочитанным из json. nullptr, если json не прочитался.
Node* replaceNode(EditorSession& session, Node& node, const Json& json)
{
    return replaceNode(session, node, readBranch(session, json));
}

// Правка экземпляра с корнем rootId: он заменяется веткой из json.
std::unique_ptr<Command> replaceCommand(NodeId rootId, Json json, std::optional<PrefabFileChange> file = std::nullopt)
{
    const bool rebuild = file.has_value();
    auto change = [rootId, json = std::move(json), rebuild](EditorSession& session) {
        Node* root = session.findNode(rootId);
        if (!root || !replaceNode(session, *root, json)) {
            return false;
        }
        // Префаб переписан: его экземпляры и содержащие его - по новым данным.
        if (rebuild) {
            session.prefabEditing().rebuildChanged();
        }

        return true;
    };

    return std::make_unique<SceneChangeCommand>(std::move(change), std::move(file));
}

const Property* findProperty(const TypeRegistry& registry, std::string_view typeName, std::string_view name)
{
    const TypeInfo* type = registry.find(typeName);
    if (!type) {
        return nullptr;
    }
    const auto properties = registry.properties(*type);
    const auto it = std::ranges::find(properties, name, &Property::name);

    return it != properties.end() ? *it : nullptr;
}

Component* findComponent(const Node& node, ComponentId id)
{
    for (const auto& component : node.components()) {
        if (component->id() == id) {
            return component.get();
        }
    }

    return nullptr;
}

// Переопределения record (в пространстве префаба) - узлам префаба,
// открытого сценой prefab: ID её узлов и есть ID в префабе.
void applyToPrefabScene(Scene& prefab, const Json& record, const TypeRegistry& registry, const PrefabSources& sources)
{
    const auto warnMissing = [](const Json& entry) {
        log::warn("Editor: the target of an override is not in the prefab: {}", entry.dump());
    };
    for (const Json& entry : arrayAt(record, keys::kOverrides)) {
        Node* node = prefab.find(idAt(entry, keys::kNode));
        Component* component = node && entry.contains(keys::kComponent)
                                   ? findComponent(*node, idAt(entry, keys::kComponent))
                                   : nullptr;
        const std::string name = entry.value(keys::kProperty, std::string());
        const Property* property = !node ? nullptr
                                         : findProperty(
                                               registry,
                                               component ? component->typeName() : node->typeName(),
                                               name
                                           );
        const auto value = property && entry.contains(keys::kValue) ? valueFromJson(entry[keys::kValue], *property)
                                                                    : std::nullopt;
        if (!value || (entry.contains(keys::kComponent) && !component)) {
            warnMissing(entry);
            continue;
        }
        if (component) {
            property->set(*component, *value);
        } else {
            property->set(*node, *value);
        }
    }
    for (const Json& entry : arrayAt(record, keys::kRemovedComponents)) {
        Node* node = prefab.find(idAt(entry, keys::kNode));
        Component* component = node ? findComponent(*node, idAt(entry, keys::kComponent)) : nullptr;
        if (!component || !node->removeComponent(*component)) {
            warnMissing(entry);
        }
    }
    for (const Json& entry : arrayAt(record, keys::kAddedComponents)) {
        Node* node = prefab.find(idAt(entry, keys::kNode));
        const auto component = entry.find(keys::kComponent);
        if (!node || component == entry.end()) {
            warnMissing(entry);
            continue;
        }
        // Компонент читается тем же кодом, что сцена: на узле того же типа,
        // откуда его забирают со своим ID.
        const Json carrier{
            {keys::kType, node->typeName()},
            {keys::kId, idToJson(newObjectId())},
            {keys::kComponents, Json::array({*component})},
        };
        auto holder = SceneBranches::read(registry, carrier, BranchIds::FromFile, &sources);
        Component* read = holder ? findComponent(*holder, idAt(*component, keys::kId)) : nullptr;
        auto taken = read ? holder->removeComponent(*read) : nullptr;
        if (!taken) {
            warnMissing(entry);
            continue;
        }
        node->addComponent(std::move(taken));
    }
    for (const Json& entry : arrayAt(record, keys::kAdded)) {
        Node* parent = prefab.find(idAt(entry, keys::kParent));
        const auto json = entry.find(keys::kNode);
        auto node = parent && json != entry.end() ? SceneBranches::read(registry, *json, BranchIds::FromFile, &sources)
                                                  : nullptr;
        if (!node) {
            warnMissing(entry);
            continue;
        }
        parent->addChild(std::move(node));
    }
    for (const Json& entry : arrayAt(record, keys::kRemoved)) {
        Node* node = prefab.find(idFromJson(entry).value_or(0));
        if (!node || !node->parent()) {
            warnMissing(entry);
            continue;
        }
        node->parent()->removeChild(*node);
    }
}

} // namespace

Node* instanceRootOf(Node& node)
{
    return const_cast<Node*>(instanceRootOf(static_cast<const Node&>(node)));
}

const Node* instanceRootOf(const Node& node)
{
    for (const Node* current = &node; current; current = current->parent()) {
        if (current->prefabInstance()) {
            return current;
        }
    }

    return nullptr;
}

InstanceOverrides::InstanceOverrides(const Node& root, const TypeRegistry& registry)
    : m_root(root)
    , m_instance(*root.prefabInstance())
    , m_registry(registry)
    , m_record(SceneBranches::write(registry, root))
{
}

ComponentId InstanceOverrides::sourceOf(const Node& node, const Component& component) const
{
    return m_instance.base ? sourceComponent(*m_instance.base, m_instance.id, node.prefabLink().source, component.id())
                           : 0;
}

bool InstanceOverrides::property(const Node& node, const Component* component, std::string_view property) const
{
    if (!m_instance.base || node.prefabLink().instance != m_instance.id) {
        return false;
    }
    const ComponentId source = component ? sourceOf(node, *component) : 0;
    if (component && source == 0) {
        return false;
    }
    const EntryFilter shown = editable(m_root, m_registry);

    return std::ranges::any_of(arrayAt(m_record, keys::kOverrides), [&](const Json& entry) {
        return idAt(entry, keys::kNode) == node.prefabLink().source && idAt(entry, keys::kComponent) == source
               && entry.value(keys::kProperty, std::string()) == property && shown(keys::kOverrides, entry);
    });
}

bool InstanceOverrides::addedNode(const Node& node) const
{
    return &node != &m_root && node.prefabLink().instance != m_instance.id;
}

bool InstanceOverrides::addedComponent(const Node& node, const Component& component) const
{
    return m_instance.base && node.prefabLink().instance == m_instance.id && sourceOf(node, component) == 0;
}

bool InstanceOverrides::componentOverridden(const Node& node, const Component& component) const
{
    if (!m_instance.base || node.prefabLink().instance != m_instance.id) {
        return false;
    }
    const ComponentId source = sourceOf(node, component);
    const EntryFilter shown = editable(m_root, m_registry);

    return source != 0 && std::ranges::any_of(arrayAt(m_record, keys::kOverrides), [&](const Json& entry) {
               return idAt(entry, keys::kNode) == node.prefabLink().source && idAt(entry, keys::kComponent) == source
                      && shown(keys::kOverrides, entry);
           });
}

bool InstanceOverrides::any() const
{
    return m_instance.base && hasEntries(filterRecord(m_record, editable(m_root, m_registry)));
}

std::size_t InstanceOverrides::unused() const
{
    std::size_t count = 0;
    for (const char* kind :
         {keys::kOverrides, keys::kAddedComponents, keys::kRemovedComponents, keys::kAdded, keys::kRemoved}) {
        count += arrayAt(m_instance.unused, kind).size();
    }

    return m_instance.base ? count : 0;
}

bool PrefabEditing::canInstantiate(ResourceId prefab) const
{
    if (!m_session.editingPrefab()) {
        return true;
    }

    return !prefabContains(prefab, m_session.document().id(), PrefabSources::of(m_session.services()));
}

namespace {

// Узел экземпляра из префаба: его корень и ID в префабе; nullopt - узел не из
// префаба (добавлен) или вне экземпляров.
struct InstanceNode {
    Node* root = nullptr;
    NodeId source = 0;
};

std::optional<InstanceNode> instanceNodeOf(Node& node)
{
    Node* root = instanceRootOf(node);
    const PrefabInstance* instance = root ? root->prefabInstance() : nullptr;
    if (!instance || !instance->base || node.prefabLink().instance != instance->id) {
        return std::nullopt;
    }

    return InstanceNode{root, node.prefabLink().source};
}

// Переопределения свойства property узла (компонента) экземпляра.
EntryFilter propertyEntries(NodeId source, ComponentId component, std::string property)
{
    return [=](std::string_view kind, const Json& entry) {
        return kind == keys::kOverrides && idAt(entry, keys::kNode) == source
               && idAt(entry, keys::kComponent) == component && entry.value(keys::kProperty, std::string()) == property;
    };
}

// Переопределения компонента: его свойства, а у добавленного - он сам.
EntryFilter componentEntries(NodeId source, ComponentId component, ComponentId live)
{
    return [=](std::string_view kind, const Json& entry) {
        if (idAt(entry, keys::kNode) != source) {
            return false;
        }
        if (kind == keys::kOverrides) {
            return component != 0 && idAt(entry, keys::kComponent) == component;
        }
        const auto added = entry.find(keys::kComponent);

        return kind == keys::kAddedComponents && component == 0 && added != entry.end()
               && idAt(*added, keys::kId) == live;
    };
}

} // namespace

bool PrefabEditing::revertProperty(Node& node, Component* component, const Property& property)
{
    const auto target = instanceNodeOf(node);
    if (m_session.playing() || !target) {
        return false;
    }
    const PrefabInstance& instance = *target->root->prefabInstance();
    const ComponentId source = component ? sourceComponent(*instance.base, instance.id, target->source, component->id())
                                         : 0;
    if (component && source == 0) {
        return false;
    }
    const EntryFilter drop = propertyEntries(target->source, source, property.name());
    const Json record = SceneBranches::write(m_session.registry(), *target->root);
    const Json kept = filterRecord(record, [&](std::string_view kind, const Json& entry) {
        return !drop(kind, entry);
    });
    if (kept == record) {
        return false;
    }

    return m_session.execute(replaceCommand(target->root->id(), kept));
}

bool PrefabEditing::revertComponent(Node& node, Component& component)
{
    const auto target = instanceNodeOf(node);
    if (m_session.playing() || !target) {
        return false;
    }
    const PrefabInstance& instance = *target->root->prefabInstance();
    const ComponentId source = sourceComponent(*instance.base, instance.id, target->source, component.id());
    const EntryFilter drop = componentEntries(target->source, source, component.id());
    const Json record = SceneBranches::write(m_session.registry(), *target->root);
    const Json kept = filterRecord(record, [&](std::string_view kind, const Json& entry) {
        return !drop(kind, entry);
    });
    if (kept == record) {
        return false;
    }

    return m_session.execute(replaceCommand(target->root->id(), kept));
}

bool PrefabEditing::revertInstance(Node& root)
{
    const PrefabInstance* instance = root.prefabInstance();
    if (m_session.playing() || !instance || !instance->base) {
        return false;
    }
    const EntryFilter shown = editable(root, m_session.registry());
    const Json record = SceneBranches::write(m_session.registry(), root);
    const Json kept = filterRecord(record, [&](std::string_view kind, const Json& entry) {
        return !shown(kind, entry);
    });
    if (kept == record) {
        return false;
    }

    return m_session.execute(replaceCommand(root.id(), kept));
}

bool PrefabEditing::removeUnused(Node& root)
{
    const PrefabInstance* instance = root.prefabInstance();
    if (m_session.playing() || !instance || !instance->base) {
        return false;
    }
    const Json record = SceneBranches::write(m_session.registry(), root);
    const Json kept = filterRecord(record, [&](std::string_view kind, const Json& entry) {
        return !unusedEntry(*instance, kind, entry);
    });
    if (kept == record) {
        return false;
    }

    return m_session.execute(replaceCommand(root.id(), kept));
}

bool PrefabEditing::canApply(const Node& root) const
{
    const PrefabInstance* instance = root.prefabInstance();
    const auto& project = m_session.project();
    const auto path = instance && instance->base && project ? project->resources().path(instance->prefab)
                                                            : std::nullopt;

    return path && path->extension() == kPrefabFileExtension;
}

bool PrefabEditing::applyEntries(Node& root, const EntryFilter& select)
{
    EditorSession& session = m_session;
    if (session.playing() || !canApply(root)) {
        return false;
    }
    const TypeRegistry& registry = session.registry();
    const PrefabInstance& instance = *root.prefabInstance();
    const EntryFilter shown = editable(root, registry);
    const auto applied = [&](std::string_view kind, const Json& entry) {
        return shown(kind, entry) && select(kind, entry);
    };
    const Json record = SceneBranches::write(registry, root);
    const Json subset = filterRecord(record, applied);
    if (!hasEntries(subset)) {
        return false;
    }
    const PrefabSources sources = PrefabSources::of(session.services());
    for (const Json& entry : arrayAt(subset, keys::kAdded)) {
        for (const ResourceId nested : recordPrefabs(entry.value(keys::kNode, Json()))) {
            if (prefabContains(nested, instance.prefab, sources)) {
                log::error("Editor: the prefab would contain itself, the overrides are not applied");

                return false;
            }
        }
    }

    const auto file = session.project()->directory() / *session.project()->resources().path(instance.prefab);
    Scene prefab{session.services()};
    if (!session.serializer().load(prefab, file)) {
        return false;
    }
    applyToPrefabScene(prefab, subset, registry, sources);
    Json after = Json::parse(session.serializer().toJson(prefab));
    const Json rest = filterRecord(record, [&](std::string_view kind, const Json& entry) {
        return !applied(kind, entry);
    });

    return session.execute(replaceCommand(root.id(), rest, PrefabFileChange{instance.prefab, file, std::move(after)}));
}

bool PrefabEditing::applyProperty(Node& node, Component* component, const Property& property)
{
    const auto target = instanceNodeOf(node);
    if (!target) {
        return false;
    }
    const PrefabInstance& instance = *target->root->prefabInstance();
    const ComponentId source = component ? sourceComponent(*instance.base, instance.id, target->source, component->id())
                                         : 0;
    if (component && source == 0) {
        return false;
    }

    return applyEntries(*target->root, propertyEntries(target->source, source, property.name()));
}

bool PrefabEditing::applyComponent(Node& node, Component& component)
{
    const auto target = instanceNodeOf(node);
    if (!target) {
        return false;
    }
    const PrefabInstance& instance = *target->root->prefabInstance();
    const ComponentId source = sourceComponent(*instance.base, instance.id, target->source, component.id());

    return applyEntries(*target->root, componentEntries(target->source, source, component.id()));
}

bool PrefabEditing::applyInstance(Node& root)
{
    return applyEntries(root, [](std::string_view, const Json&) {
        return true;
    });
}

bool PrefabEditing::unpack(Node& root)
{
    if (m_session.playing() || !root.prefabInstance()) {
        return false;
    }

    return m_session.execute(replaceCommand(root.id(), SceneBranches::writeUnpacked(m_session.registry(), root)));
}

Node* PrefabEditing::saveAsPrefab(Node& node, const std::filesystem::path& folder, std::string_view name)
{
    auto& project = m_session.project();
    if (m_session.playing() || !project) {
        return nullptr;
    }
    const TypeRegistry& registry = m_session.registry();
    ProjectFiles files(*project);
    const auto relative = files.createPrefab(folder, name, SceneBranches::write(registry, node));
    if (!relative) {
        return nullptr;
    }
    const ResourceId prefab = project->resources().id(*relative);
    const NodeId nodeId = node.id();
    NodeId created = 0;
    auto change = [&, nodeId, prefab](EditorSession& session) {
        Node* old = session.findNode(nodeId);
        const PrefabSources sources = PrefabSources::of(session.services());
        auto instance = old ? SceneBranches::instantiate(registry, prefab, sources) : nullptr;
        if (!instance) {
            return false;
        }
        const PrefabInstance& data = *instance->prefabInstance();
        // Узлы ветки - узлы префаба: ссылки на них ведут теперь в экземпляр.
        const PrefabIdMapping ids = PrefabIdMapping::of(*data.base, data.id, false);
        const Json json = SceneBranches::write(registry, *instance);
        Node* root = replaceNode(session, *old, json);
        if (!root) {
            return false;
        }
        remapReferences(*session.scene().root(), ids, registry);
        created = root->id();
        session.selection().select(created);

        return true;
    };
    if (!m_session.execute(std::make_unique<SceneChangeCommand>(change))) {
        return nullptr;
    }

    return m_session.findNode(created);
}

bool PrefabEditing::rebuildChanged()
{
    if (m_session.playing()) {
        return false;
    }
    const TypeRegistry& registry = m_session.registry();
    const PrefabSources sources = PrefabSources::of(m_session.services());
    bool rebuilt = false;
    std::function<void(Node&)> visit = [&](Node& node) {
        if (const PrefabInstance* instance = node.prefabInstance()) {
            const bool stale = instance->base
                                   ? sources.bases->find(instance->prefab, registry.revision(), sources.cache)
                                         != instance->base
                                   : !sources.cache.loadedKind(instance->prefab).empty();
            if (stale) {
                // Запись считается по прежним данным, развёртывается по новым.
                auto fresh = readBranch(m_session, SceneBranches::write(registry, node));
                const PrefabInstance* expanded = fresh ? fresh->prefabInstance() : nullptr;
                // Префаб пропал (файл удалили): экземпляр остаётся, каким был,
                // и сохраняется своей записью.
                if (expanded && (expanded->base || !instance->base)) {
                    rebuilt = replaceNode(m_session, node, std::move(fresh)) != nullptr || rebuilt;
                }

                return;
            }
        }
        for (std::size_t i = 0; i < node.children().size(); ++i) {
            visit(*node.children()[i]);
        }
    };
    if (Node* root = m_session.scene().root()) {
        visit(*root);
    }
    if (rebuilt) {
        m_session.refreshModified();
    }

    return rebuilt;
}

} // namespace yqlora::editor
