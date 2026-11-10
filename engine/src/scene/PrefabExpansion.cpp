#include "yqlora/scene/PrefabExpansion.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/JsonId.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/import/ModelPrefab.hpp"
#include "yqlora/project/Project.hpp"
#include "yqlora/reflection/PropertyJson.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/MissingComponent.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/ObjectId.hpp"
#include "yqlora/scene/SceneBranches.hpp"
#include "yqlora/scene/SceneFileKeys.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>

namespace yqlora {

namespace {

using Json = nlohmann::json;
namespace keys = scene_keys;

// Виды переопределений записи, в порядке, в котором их пишет computeOverrides.
constexpr const char* kOverrideKinds[] = {
    keys::kOverrides,
    keys::kAddedComponents,
    keys::kRemovedComponents,
    keys::kAdded,
    keys::kRemoved,
};

std::uint64_t idAt(const Json& object, const char* key)
{
    const auto it = object.find(key);

    return it != object.end() ? idFromJson(*it).value_or(0) : 0;
}

// Массив по ключу; пустой, если ключа нет или там не массив.
const Json& arrayAt(const Json& object, const char* key)
{
    static const Json kEmpty = Json::array();
    const auto it = object.find(key);

    return it != object.end() && it->is_array() ? *it : kEmpty;
}

const Json& childrenOf(const Json& node)
{
    return arrayAt(node, keys::kChildren);
}

const Json& componentsOf(const Json& node)
{
    return arrayAt(node, keys::kComponents);
}

const Json& propertiesOf(const Json& object)
{
    static const Json kEmpty = Json::object();
    const auto it = object.find(keys::kProperties);

    return it != object.end() && it->is_object() ? *it : kEmpty;
}

void addMapping(PrefabIdMapping& mapping, const Json& node, NodeId instance, bool inverse)
{
    const NodeId source = idAt(node, keys::kId);
    const NodeId derived = deriveId(instance, source);
    mapping.nodes[inverse ? derived : source] = inverse ? source : derived;
    for (const Json& component : componentsOf(node)) {
        const ComponentId id = idAt(component, keys::kId);
        const ComponentId derivedId = deriveId(instance, id);
        mapping.components[inverse ? std::pair(derived, derivedId) : std::pair(source, id)] = inverse ? id : derivedId;
    }
    for (const Json& child : childrenOf(node)) {
        addMapping(mapping, child, instance, inverse);
    }
}

// Свойство может держать ссылки на узлы и компоненты.
bool mayHoldReferences(const Property& property)
{
    switch (property.type()) {
    case PropertyType::Node:
    case PropertyType::Component:
    case PropertyType::List:
    case PropertyType::Record: return true;
    default: return false;
    }
}

bool remapValueIn(PropertyValue& value, const PrefabIdMapping& ids)
{
    return std::visit(
        [&](auto& held) {
            using T = std::decay_t<decltype(held)>;
            if constexpr (std::is_same_v<T, NodeRef>) {
                const NodeId mapped = ids.node(held.id);

                return std::exchange(held.id, mapped) != mapped;
            } else if constexpr (std::is_same_v<T, ComponentRef<>>) {
                const ComponentRef<> mapped{ids.node(held.node), ids.component(held.node, held.component)};

                return std::exchange(held, mapped) != mapped;
            } else if constexpr (std::is_same_v<T, ListValue> || std::is_same_v<T, RecordValue>) {
                bool changed = false;
                auto& items = [&]() -> auto& {
                    if constexpr (std::is_same_v<T, ListValue>) {
                        return held.items;
                    } else {
                        return held.fields;
                    }
                }();
                for (PropertyValue& item : items) {
                    changed = remapValueIn(item, ids) || changed;
                }

                return changed;
            } else {
                return false;
            }
        },
        value
    );
}

// Ссылки в свойствах объекта типа typeName. Свойства неизвестного типа
// (компонент-заглушка) не трогаются: их значений не прочитать.
void remapProperties(Json& object, const TypeRegistry& registry, const PrefabIdMapping& ids)
{
    const auto type = object.find(keys::kType);
    const TypeInfo* info = type != object.end() && type->is_string() ? registry.find(type->get<std::string>())
                                                                     : nullptr;
    const auto properties = object.find(keys::kProperties);
    if (!info || properties == object.end() || !properties->is_object()) {
        return;
    }
    for (const Property* property : registry.properties(*info)) {
        if (!mayHoldReferences(*property)) {
            continue;
        }
        // Значение лежит в нашем изменяемом объекте, findPropertyJson лишь находит его.
        auto* found = const_cast<Json*>(findPropertyJson(*properties, *property));
        auto value = found ? valueFromJson(*found, *property) : std::nullopt;
        if (value && remapValueIn(*value, ids)) {
            *found = valueToJson(*property, *value);
        }
    }
}

// ID и ссылки ветки в другое пространство. Записи экземпляров не трогаются:
// их ID и переопределения - в своих пространствах.
void translateBranch(Json& node, const PrefabIdMapping& ids, const TypeRegistry& registry)
{
    if (isPrefabRecord(node)) {
        return;
    }
    const NodeId before = idAt(node, keys::kId);
    if (before != 0) {
        node[keys::kId] = idToJson(ids.node(before));
    }
    remapProperties(node, registry, ids);
    if (const auto components = node.find(keys::kComponents); components != node.end() && components->is_array()) {
        for (Json& component : *components) {
            if (const ComponentId id = idAt(component, keys::kId); id != 0) {
                component[keys::kId] = idToJson(ids.component(before, id));
            }
            remapProperties(component, registry, ids);
        }
    }
    if (const auto children = node.find(keys::kChildren); children != node.end() && children->is_array()) {
        for (Json& child : *children) {
            translateBranch(child, ids, registry);
        }
    }
}

void indexNodes(Json& node, std::unordered_map<NodeId, Json*>& index)
{
    index[idAt(node, keys::kId)] = &node;
    if (const auto children = node.find(keys::kChildren); children != node.end() && children->is_array()) {
        for (Json& child : *children) {
            indexNodes(child, index);
        }
    }
}

void indexNodes(const Json& node, std::unordered_map<NodeId, const Json*>& index)
{
    index[idAt(node, keys::kId)] = &node;
    for (const Json& child : childrenOf(node)) {
        indexNodes(child, index);
    }
}

Json* findComponent(Json& node, ComponentId id)
{
    if (const auto components = node.find(keys::kComponents); components != node.end() && components->is_array()) {
        for (Json& component : *components) {
            if (idAt(component, keys::kId) == id) {
                return &component;
            }
        }
    }

    return nullptr;
}

// Узлы и удаление в дереве после правок свойств: удалённые дети убираются,
// добавленные дописываются, по всему дереву за один проход, так что указатели
// на узлы до него не нужны.
struct Restructure {
    const std::unordered_map<NodeId, std::vector<const Json*>>& added;
    const std::unordered_set<NodeId>& removed;

    void apply(Json& node) const
    {
        Json& children = node[keys::kChildren];
        if (!children.is_array()) {
            children = Json::array();
        }
        Json kept = Json::array();
        for (Json& child : children) {
            if (!removed.contains(idAt(child, keys::kId))) {
                apply(child);
                kept.push_back(std::move(child));
            }
        }
        if (const auto it = added.find(idAt(node, keys::kId)); it != added.end()) {
            for (const Json* entry : it->second) {
                kept.push_back((*entry)[keys::kNode]);
            }
        }
        children = std::move(kept);
    }
};

void addUnused(Json& unused, const char* kind, const Json& entry)
{
    Json& list = unused[kind];
    if (!list.is_array()) {
        list = Json::array();
    }
    list.push_back(entry);
}

// Сравнение одного узла экземпляра с его узлом в префабе. Оба - в
// пространстве префаба.
struct OverrideDiff {
    const TypeRegistry& registry;
    const std::unordered_map<NodeId, const Json*>& baseNodes;
    Json& record;

    void properties(const Json& base, const Json& live, NodeId node, ComponentId component, bool own)
    {
        const Json& baseProperties = propertiesOf(base);
        for (const auto& [name, value] : propertiesOf(live).items()) {
            // Своё у корня - его имя: пишется всегда.
            const bool always = own && (component != 0 || name == keys::kNameProperty);
            const auto it = baseProperties.find(name);
            if (!always && it != baseProperties.end() && *it == value) {
                continue;
            }
            Json entry{{keys::kNode, idToJson(node)}};
            if (component != 0) {
                entry[keys::kComponent] = idToJson(component);
            }
            entry[keys::kProperty] = name;
            entry[keys::kValue] = value;
            add(keys::kOverrides, std::move(entry));
        }
    }

    bool builtin(const Json& component) const
    {
        const TypeInfo* type = registry.find(component.value(keys::kType, std::string()));

        return type && type->builtin();
    }

    void node(const Json& base, const Json& live, bool root)
    {
        const NodeId id = idAt(base, keys::kId);
        // Имя корня у экземпляра своё (вместе с положением, см. ниже).
        properties(base, live, id, 0, root);

        std::unordered_map<ComponentId, const Json*> baseComponents;
        for (const Json& component : componentsOf(base)) {
            baseComponents[idAt(component, keys::kId)] = &component;
        }
        std::unordered_set<ComponentId> present;
        for (const Json& component : componentsOf(live)) {
            const ComponentId componentId = idAt(component, keys::kId);
            const auto it = baseComponents.find(componentId);
            if (it == baseComponents.end()) {
                add(keys::kAddedComponents, {{keys::kNode, idToJson(id)}, {keys::kComponent, component}});
                continue;
            }
            present.insert(componentId);
            // Встроенные компоненты корня (положение) у экземпляра свои.
            properties(*it->second, component, id, componentId, root && builtin(component));
        }
        for (const Json& component : componentsOf(base)) {
            const ComponentId componentId = idAt(component, keys::kId);
            if (!present.contains(componentId)) {
                add(keys::kRemovedComponents, {{keys::kNode, idToJson(id)}, {keys::kComponent, idToJson(componentId)}});
            }
        }

        std::unordered_map<NodeId, const Json*> liveChildren;
        for (const Json& child : childrenOf(live)) {
            const NodeId childId = isPrefabRecord(child) ? 0 : idAt(child, keys::kId);
            if (childId != 0 && baseNodes.contains(childId)) {
                liveChildren[childId] = &child;
            } else {
                add(keys::kAdded, {{keys::kParent, idToJson(id)}, {keys::kNode, child}});
            }
        }
        for (const Json& child : childrenOf(base)) {
            const NodeId childId = idAt(child, keys::kId);
            if (const auto it = liveChildren.find(childId); it != liveChildren.end()) {
                this->node(child, *it->second, false);
                liveChildren.erase(it);
            } else {
                add(keys::kRemoved, idToJson(childId));
            }
        }
        // Узел префаба у другого родителя: экземпляр переносить узлы префаба
        // не умеет, иначе после загрузки узел был бы дважды.
        for (const auto& [childId, child] : liveChildren) {
            log::warn(
                "Prefab: node '{}' moved inside an instance is not saved",
                propertiesOf(*child).value(keys::kNameProperty, std::string())
            );
        }
    }

    void add(const char* kind, Json entry)
    {
        addUnused(record, kind, entry);
    }
};

// Префабы записей экземпляров в ветке: записей узлов и записей среди
// добавленных в экземпляры узлов.
void collectRecords(const Json& node, std::vector<ResourceId>& prefabs)
{
    if (isPrefabRecord(node)) {
        prefabs.push_back(recordPrefab(node));
        for (const Json& entry : arrayAt(node, keys::kAdded)) {
            if (const auto added = entry.find(keys::kNode); added != entry.end()) {
                collectRecords(*added, prefabs);
            }
        }

        return;
    }
    for (const Json& child : childrenOf(node)) {
        collectRecords(child, prefabs);
    }
}

void freshenNode(Json& node)
{
    if (isPrefabRecord(node)) {
        freshenRecord(node);

        return;
    }
    node[keys::kId] = idToJson(newObjectId());
    if (const auto components = node.find(keys::kComponents); components != node.end() && components->is_array()) {
        for (Json& component : *components) {
            component[keys::kId] = idToJson(newObjectId());
        }
    }
    if (const auto children = node.find(keys::kChildren); children != node.end() && children->is_array()) {
        for (Json& child : *children) {
            freshenNode(child);
        }
    }
}

} // namespace

NodeId PrefabIdMapping::node(NodeId id) const
{
    const auto it = nodes.find(id);

    return it != nodes.end() ? it->second : id;
}

ComponentId PrefabIdMapping::component(NodeId node, ComponentId id) const
{
    const auto it = components.find({node, id});

    return it != components.end() ? it->second : id;
}

PrefabIdMapping PrefabIdMapping::of(const Json& base, NodeId instance, bool inverse)
{
    PrefabIdMapping mapping;
    addMapping(mapping, base, instance, inverse);

    return mapping;
}

bool remapReferences(PropertyValue& value, const PrefabIdMapping& ids)
{
    return remapValueIn(value, ids);
}

void remapReferences(Node& branch, const PrefabIdMapping& ids, const TypeRegistry& registry)
{
    const auto remapObject = [&](auto& object) {
        const TypeInfo* type = registry.find(object.typeName());
        if (!type) {
            return;
        }
        for (const Property* property : registry.properties(*type)) {
            if (!mayHoldReferences(*property)) {
                continue;
            }
            PropertyValue value = property->get(object);
            if (remapValueIn(value, ids)) {
                property->set(object, value);
            }
        }
    };
    remapObject(branch);
    for (const auto& component : branch.components()) {
        if (!dynamic_cast<const MissingComponent*>(component.get())) {
            remapObject(*component);
        }
    }
    for (const auto& child : branch.children()) {
        remapReferences(*child, ids, registry);
    }
}

bool prefabContains(ResourceId prefab, ResourceId target, const PrefabSources& sources)
{
    std::vector<ResourceId> visited;
    const std::function<bool(ResourceId)> contains = [&](ResourceId current) {
        if (current == target) {
            return true;
        }
        if (std::ranges::find(visited, current) != visited.end()) {
            return false;
        }
        visited.push_back(current);
        const auto tree = sources.cache.share<PrefabData>(current);
        if (!tree) {
            // Модель экземпляров не содержит, не префаб - тем более.
            return false;
        }
        std::vector<ResourceId> records;
        collectRecords(tree->root, records);

        return std::ranges::any_of(records, contains);
    };

    return contains(prefab);
}

std::vector<ResourceId> recordPrefabs(const Json& branch)
{
    std::vector<ResourceId> prefabs;
    collectRecords(branch, prefabs);

    return prefabs;
}

ComponentId sourceComponent(const Json& base, NodeId instance, NodeId sourceNode, ComponentId live)
{
    std::unordered_map<NodeId, const Json*> nodes;
    indexNodes(base, nodes);
    const auto it = nodes.find(sourceNode);
    if (it == nodes.end()) {
        return 0;
    }
    for (const Json& component : componentsOf(*it->second)) {
        const ComponentId id = idAt(component, keys::kId);
        if (deriveId(instance, id) == live) {
            return id;
        }
    }

    return 0;
}

Json filterRecord(const Json& record, const std::function<bool(std::string_view kind, const Json& entry)>& keep)
{
    Json result = record;
    for (const char* kind : kOverrideKinds) {
        result.erase(kind);
        for (const Json& entry : arrayAt(record, kind)) {
            if (keep(kind, entry)) {
                addUnused(result, kind, entry);
            }
        }
    }

    return result;
}

PrefabSources PrefabSources::of(const EngineServices& services)
{
    return {
        services.resources(),
        services.project() ? &services.project()->resources() : nullptr,
        &services.prefabBases(),
    };
}

bool isPrefabRecord(const Json& node)
{
    return node.is_object() && node.contains(keys::kPrefab);
}

ResourceId recordPrefab(const Json& record)
{
    return idAt(record, keys::kPrefab);
}

NodeId recordInstance(const Json& record)
{
    return idAt(record, keys::kId);
}

std::optional<std::string> recordName(const Json& record)
{
    for (const Json& entry : arrayAt(record, keys::kOverrides)) {
        const auto value = entry.find(keys::kValue);
        if (!entry.contains(keys::kComponent) && entry.value(keys::kProperty, std::string()) == keys::kNameProperty
            && value != entry.end() && value->is_string()) {
            return value->get<std::string>();
        }
    }

    return std::nullopt;
}

std::shared_ptr<const Json> prefabTree(ResourceId prefab, const PrefabSources& sources, const TypeRegistry& registry)
{
    std::shared_ptr<const Json> tree;
    if (const auto data = sources.cache.share<PrefabData>(prefab)) {
        // Корень живёт, пока живут данные файла.
        tree = std::shared_ptr<const Json>(data, &data->root);
    } else if (auto branch = modelBranch(prefab, sources.cache, sources.resources, registry)) {
        tree = std::make_shared<const Json>(std::move(*branch));
    }
    if (!tree || !tree->is_object()) {
        log::error("Prefab {:016x} is not a prefab or a model, or is not loaded", prefab);

        return nullptr;
    }

    return tree;
}

std::optional<Json> resolvePrefabs(
    Json branch,
    const PrefabSources& sources,
    const TypeRegistry& registry,
    std::vector<ResourceId>& chain,
    std::vector<ResourceId>* used
)
{
    if (isPrefabRecord(branch)) {
        const ResourceId prefab = recordPrefab(branch);
        const NodeId instance = recordInstance(branch);
        if (prefab == 0 || instance == 0) {
            log::error("Prefab: instance without a prefab or an ID, skipped");

            return std::nullopt;
        }
        if (std::ranges::find(chain, prefab) != chain.end()) {
            log::error("Prefab {:016x} contains itself, the instance is skipped", prefab);

            return std::nullopt;
        }
        // Записан и тот, что не развернулся: когда он появится, дерево устареет.
        if (used) {
            used->push_back(prefab);
        }
        const auto tree = prefabTree(prefab, sources, registry);
        if (!tree) {
            return std::nullopt;
        }
        chain.push_back(prefab);
        const auto base = resolvePrefabs(*tree, sources, registry, chain, used);
        chain.pop_back();
        if (!base) {
            return std::nullopt;
        }
        // Добавленные в экземпляр узлы - в пространстве этой ветки, их записи
        // разворачиваются ниже с её цепочкой.
        branch = expandPrefab(*base, branch, instance, registry).tree;
    }
    if (const auto children = branch.find(keys::kChildren); children != branch.end() && children->is_array()) {
        Json resolved = Json::array();
        for (Json& child : *children) {
            if (auto node = resolvePrefabs(std::move(child), sources, registry, chain, used)) {
                resolved.push_back(std::move(*node));
            }
        }
        *children = std::move(resolved);
    }

    return branch;
}

Json applyOverrides(Json& tree, const Json& record)
{
    Json unused = Json::object();
    std::unordered_map<NodeId, Json*> nodes;
    indexNodes(tree, nodes);
    const auto nodeOf = [&](const Json& entry) -> Json* {
        const auto it = nodes.find(idAt(entry, keys::kNode));

        return it != nodes.end() ? it->second : nullptr;
    };

    for (const Json& entry : arrayAt(record, keys::kOverrides)) {
        Json* target = nodeOf(entry);
        if (target && entry.contains(keys::kComponent)) {
            target = findComponent(*target, idAt(entry, keys::kComponent));
        }
        const auto property = entry.find(keys::kProperty);
        if (!target || property == entry.end() || !property->is_string() || !entry.contains(keys::kValue)) {
            addUnused(unused, keys::kOverrides, entry);
            continue;
        }
        (*target)[keys::kProperties][property->get<std::string>()] = entry[keys::kValue];
    }
    for (const Json& entry : arrayAt(record, keys::kRemovedComponents)) {
        Json* node = nodeOf(entry);
        Json* component = node ? findComponent(*node, idAt(entry, keys::kComponent)) : nullptr;
        if (!component) {
            addUnused(unused, keys::kRemovedComponents, entry);
            continue;
        }
        Json& components = (*node)[keys::kComponents];
        components.erase(static_cast<std::size_t>(component - &components.front()));
    }
    for (const Json& entry : arrayAt(record, keys::kAddedComponents)) {
        Json* node = nodeOf(entry);
        const auto component = entry.find(keys::kComponent);
        if (!node || component == entry.end() || !component->is_object()) {
            addUnused(unused, keys::kAddedComponents, entry);
            continue;
        }
        Json& components = (*node)[keys::kComponents];
        if (!components.is_array()) {
            components = Json::array();
        }
        components.push_back(*component);
    }

    std::unordered_map<NodeId, std::vector<const Json*>> added;
    for (const Json& entry : arrayAt(record, keys::kAdded)) {
        const auto node = entry.find(keys::kNode);
        if (!nodes.contains(idAt(entry, keys::kParent)) || node == entry.end() || !node->is_object()) {
            addUnused(unused, keys::kAdded, entry);
            continue;
        }
        added[idAt(entry, keys::kParent)].push_back(&entry);
    }
    std::unordered_set<NodeId> removed;
    const NodeId root = idAt(tree, keys::kId);
    for (const Json& entry : arrayAt(record, keys::kRemoved)) {
        const NodeId id = idFromJson(entry).value_or(0);
        // Корень убрать нельзя: на его месте стоит экземпляр.
        if (id == 0 || id == root || !nodes.contains(id)) {
            addUnused(unused, keys::kRemoved, entry);
            continue;
        }
        removed.insert(id);
    }
    Restructure{added, removed}.apply(tree);

    return unused;
}

ExpandedPrefab expandPrefab(const Json& base, const Json& record, NodeId instance, const TypeRegistry& registry)
{
    ExpandedPrefab expanded{base, Json::object()};
    expanded.unused = applyOverrides(expanded.tree, record);
    translateBranch(expanded.tree, PrefabIdMapping::of(base, instance, false), registry);

    return expanded;
}

Json computeOverrides(
    const Json& base,
    Json live,
    ResourceId prefab,
    NodeId instance,
    const Json& unused,
    const TypeRegistry& registry
)
{
    translateBranch(live, PrefabIdMapping::of(base, instance, true), registry);
    std::unordered_map<NodeId, const Json*> baseNodes;
    indexNodes(base, baseNodes);

    Json record{{keys::kPrefab, idToJson(prefab)}, {keys::kId, idToJson(instance)}};
    OverrideDiff{registry, baseNodes, record}.node(base, live, true);
    if (unused.is_object()) {
        for (const char* kind : kOverrideKinds) {
            for (const Json& entry : arrayAt(unused, kind)) {
                addUnused(record, kind, entry);
            }
        }
    }

    return record;
}

std::unordered_map<NodeId, NodeId> instanceSources(const Json& base, NodeId instance)
{
    return PrefabIdMapping::of(base, instance, true).nodes;
}

void freshenRecord(Json& record)
{
    record[keys::kId] = idToJson(newObjectId());
    if (const auto added = record.find(keys::kAdded); added != record.end() && added->is_array()) {
        for (Json& entry : *added) {
            if (const auto node = entry.find(keys::kNode); node != entry.end() && node->is_object()) {
                freshenNode(*node);
            }
        }
    }
    if (const auto added = record.find(keys::kAddedComponents); added != record.end() && added->is_array()) {
        for (Json& entry : *added) {
            if (const auto component = entry.find(keys::kComponent);
                component != entry.end() && component->is_object()) {
                (*component)[keys::kId] = idToJson(newObjectId());
            }
        }
    }
}

} // namespace yqlora
