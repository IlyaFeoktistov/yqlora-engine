#include "yqlora/scene/SceneSerializer.hpp"

#include "yqlora/scene/SceneBranches.hpp"

#include "yqlora/core/FileFormat.hpp"
#include "yqlora/core/JsonFile.hpp"
#include "yqlora/core/JsonId.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/reflection/ComponentRules.hpp"
#include "yqlora/reflection/PropertyJson.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/MissingComponent.hpp"
#include "yqlora/scene/NodeTree.hpp"
#include "yqlora/scene/PrefabBases.hpp"
#include "yqlora/scene/PrefabExpansion.hpp"
#include "yqlora/scene/PrefabInstance.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/SceneFileKeys.hpp"
#include "yqlora/scene/UnknownProperties.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <functional>
#include <memory>
#include <optional>

namespace yqlora {

namespace {

using Json = nlohmann::json;

// Переводы файла сцены со старых версий, см. FileFormat. Пока формат один, переводов нет.
constexpr int kFirstFormatVersion = 1;
constexpr std::array<Migration, 0> kMigrations{};
constexpr FileFormat kFormat{"Scene", kFirstFormatVersion, kMigrations};

using scene_keys::kChildren;
using scene_keys::kComponents;
using scene_keys::kId;
using scene_keys::kProperties;
using scene_keys::kRoot;
using scene_keys::kType;

Json componentToJson(const TypeRegistry& registry, const Component& component)
{
    if (const auto* missing = dynamic_cast<const MissingComponent*>(&component)) {
        // Данные заглушки - как в файле, ID - её действующий.
        Json data = missing->data();
        data[kId] = idToJson(missing->id());

        return data;
    }
    const TypeInfo* type = registry.find(component.typeName());
    if (!type) {
        log::error("Scene: component type '{}' is not registered, not saved", component.typeName());

        return nullptr;
    }
    Json properties = propertiesToJson(registry, *type, component);
    // Свойство с тем же именем снова есть у типа: его значение уже записано.
    Json unknown = UnknownProperties::of(component);
    for (auto& [key, value] : unknown.items()) {
        if (!properties.contains(key)) {
            properties[key] = std::move(value);
        }
    }

    return {
        {kType, type->name()},
        {kId, idToJson(component.id())},
        {kProperties, std::move(properties)},
    };
}

Json nodeToJson(const TypeRegistry& registry, const Node& node);

// Узел как есть. instance - экземпляр, внутри которого он (0 - вне
// экземпляров): его дети из того же экземпляра пишутся так же, остальные -
// своими записями, если они экземпляры.
Json plainNodeToJson(const TypeRegistry& registry, const Node& node, NodeId instance)
{
    const TypeInfo* type = registry.find(node.typeName());
    if (!type) {
        log::error("Scene: node type '{}' is not registered, node '{}' is not saved", node.typeName(), node.name());

        return nullptr;
    }

    Json result;
    result[kType] = type->name();
    result[kId] = idToJson(node.id());
    result[kProperties] = propertiesToJson(registry, *type, node);

    Json components = Json::array();
    for (const auto& component : node.components()) {
        Json componentJson = componentToJson(registry, *component);
        if (!componentJson.is_null()) {
            components.push_back(std::move(componentJson));
        }
    }
    result[kComponents] = std::move(components);

    Json children = Json::array();
    for (const auto& child : node.children()) {
        const bool inside = instance != 0 && child->prefabLink().instance == instance;
        Json childJson = inside ? plainNodeToJson(registry, *child, instance) : nodeToJson(registry, *child);
        if (!childJson.is_null()) {
            children.push_back(std::move(childJson));
        }
    }
    result[kChildren] = std::move(children);

    return result;
}

// Корень экземпляра пишется записью: префаб, ID экземпляра, переопределения.
Json nodeToJson(const TypeRegistry& registry, const Node& node)
{
    const PrefabInstance* instance = node.prefabInstance();
    if (!instance) {
        return plainNodeToJson(registry, node, 0);
    }
    if (!instance->base) {
        return instance->unused;
    }

    return computeOverrides(
        *instance->base,
        plainNodeToJson(registry, node, instance->id),
        instance->prefab,
        instance->id,
        instance->unused,
        registry
    );
}

} // namespace

// Чтение и запись сцены. Вложен в SceneSerializer, чтобы вызывать закрытый assignId.
struct SceneSerializer::Format {
    const TypeRegistry& registry;
    // ID из файла не берутся, узлы и компоненты получают новые: копия ветки
    // рядом с оригиналом (модель, брошенная в сцену второй раз).
    bool freshIds = false;
    // Откуда брать префабы экземпляров; nullptr - неоткуда, экземпляр
    // остаётся узлом-заменой со своей записью.
    const PrefabSources* sources = nullptr;

    Json write(const Scene& scene) const;
    bool readScene(Scene& scene, Json json) const;

    std::unique_ptr<Node> readNode(const Json& json) const
    {
        if (isPrefabRecord(json)) {
            return readInstance(json);
        }
        if (!json.is_object() || !json.contains(kType) || !json[kType].is_string()) {
            log::error("Scene: node without a type");

            return nullptr;
        }
        const std::string typeName = json[kType].get<std::string>();
        const TypeInfo* type = registry.find(typeName);
        if (!type || type->kind() != TypeKind::Node) {
            log::error("Scene: unknown node type '{}'", typeName);

            return nullptr;
        }
        std::unique_ptr<Node> node = type->createNode();
        if (!node) {
            log::error("Scene: node type '{}' cannot be created", typeName);

            return nullptr;
        }

        const auto id = json.contains(kId) ? idFromJson(json[kId]) : std::nullopt;
        if (id && *id != 0 && !freshIds) {
            assignId(*node, *id);
        }
        if (const auto it = json.find(kProperties); it != json.end()) {
            propertiesFromJson(registry, *type, *node, *it);
        }
        if (const auto it = json.find(kComponents); it != json.end() && it->is_array()) {
            for (const Json& componentJson : *it) {
                readComponent(*node, componentJson);
            }
        }
        addMissingRequirements(*node);
        if (const auto it = json.find(kChildren); it != json.end() && it->is_array()) {
            for (const Json& childJson : *it) {
                std::unique_ptr<Node> child = readNode(childJson);
                if (!child) {
                    return nullptr;
                }
                node->addChild(std::move(child));
            }
        }

        return node;
    }

    // Экземпляр префаба: узлы префаба с выведенными ID и применёнными
    // переопределениями. Префаб не развернулся (нет файла, цикл) - на месте
    // экземпляра узел-замена, который пишет запись обратно как была.
    std::unique_ptr<Node> readInstance(const Json& json) const
    {
        Json record = json;
        if (freshIds) {
            freshenRecord(record);
        }
        const ResourceId prefab = recordPrefab(record);
        const NodeId instance = recordInstance(record);
        const auto base = prefab != 0 && instance != 0 ? instanceBase(prefab) : nullptr;
        if (!base) {
            return placeholder(std::move(record));
        }
        ExpandedPrefab expanded = expandPrefab(*base, record, instance, registry);
        // Новые ID записи уже даны, выведенные ID берутся как есть.
        std::unique_ptr<Node> root = Format{registry, false, sources}.readNode(expanded.tree);
        if (!root) {
            return nullptr;
        }
        const auto links = instanceSources(*base, instance);
        forEachNode(*root, [&](Node& node) {
            const auto it = links.find(node.id());
            if (it != links.end() && !node.prefabLink()) {
                node.setPrefabLink({instance, it->second});
            }
        });
        root->setPrefabInstance(
            std::make_unique<PrefabInstance>(PrefabInstance{prefab, instance, base, std::move(expanded.unused)})
        );

        return root;
    }

    // Дерево префаба, из которого разворачивается экземпляр: вложенные
    // префабы развёрнуты, значения - как их пишет сцена, чтобы сравнение с
    // экземпляром не видело разницы там, где её нет (свойства по умолчанию,
    // которых нет в файле, прежние имена свойств).
    std::shared_ptr<const Json> instanceBase(ResourceId prefab) const
    {
        if (!sources) {
            return nullptr;
        }
        if (sources->bases) {
            if (auto cached = sources->bases->find(prefab, registry.revision(), sources->cache)) {
                return cached;
            }
        }
        const auto tree = prefabTree(prefab, *sources, registry);
        if (!tree) {
            return nullptr;
        }
        std::vector<ResourceId> chain{prefab};
        std::vector<ResourceId> files{prefab};
        const auto resolved = resolvePrefabs(*tree, *sources, registry, chain, &files);
        const auto nodes = resolved ? Format{registry}.readNode(*resolved) : nullptr;
        if (!nodes) {
            return nullptr;
        }
        auto base = std::make_shared<const Json>(plainNodeToJson(registry, *nodes, 0));
        if (sources->bases) {
            sources->bases->store(prefab, registry.revision(), base, files, sources->cache);
        }

        return base;
    }

    std::unique_ptr<Node> placeholder(Json record) const
    {
        log::error("Scene: prefab {:016x} is not expanded, its instance is kept as is", recordPrefab(record));
        auto node = std::make_unique<Node>(recordName(record).value_or(std::string(Node::staticTypeName())));
        const NodeId instance = recordInstance(record);
        if (instance != 0) {
            assignId(*node, instance);
        }
        node->setPrefabInstance(
            std::make_unique<PrefabInstance>(PrefabInstance{recordPrefab(record), instance, nullptr, std::move(record)})
        );

        return node;
    }

    static void forEachNode(Node& node, const std::function<void(Node&)>& visit)
    {
        visit(node);
        for (const auto& child : node.children()) {
            forEachNode(*child, visit);
        }
    }

    // Компонент неизвестного типа становится заглушкой со своими данными, а не
    // роняет сцену и не теряется: так сцена откроется и сохранится, даже если
    // модуль игры не собран или скрипт удалили.
    void readComponent(Node& node, const Json& json) const
    {
        if (!json.is_object() || !json.contains(kType) || !json[kType].is_string()) {
            log::warn("Scene: component without a type on node '{}', skipped", node.name());

            return;
        }
        const std::string typeName = json[kType].get<std::string>();
        const TypeInfo* type = registry.find(typeName);
        if (!type || type->kind() != TypeKind::Component) {
            log::warn("Scene: unknown component type '{}' on node '{}', kept as missing", typeName, node.name());
            Component& missing = node.addComponent(std::make_unique<MissingComponent>(json));
            readComponentId(node, missing, json);

            return;
        }

        Component* component = nullptr;
        if (type->creatable()) {
            if (!canAttach(registry, *type, node)) {
                log::error(
                    "Scene: component '{}' does not fit node '{}' of type '{}', skipped",
                    typeName,
                    node.name(),
                    node.typeName()
                );

                return;
            }
            component = &node.addComponent(type->createComponent());
        } else {
            // Встроенный компонент узел уже создал сам, только заполняем его.
            for (const auto& existing : node.components()) {
                if (existing->typeName() == typeName) {
                    component = existing.get();
                    break;
                }
            }
        }
        if (!component) {
            log::warn("Scene: component '{}' cannot be added to node '{}', skipped", typeName, node.name());

            return;
        }
        readComponentId(node, *component, json);
        readProperties(*type, *component, json);
    }

    void readProperties(const TypeInfo& type, Component& component, const Json& json) const
    {
        if (const auto it = json.find(kProperties); it != json.end()) {
            propertiesFromJson(registry, type, component, *it);
            UnknownProperties::set(component, unknownPropertiesJson(registry, type, *it));
        }
    }

    // Заглушки, чей тип снова есть в реестре, - настоящими компонентами на том же месте.
    std::size_t revive(Node& node) const
    {
        std::size_t revived = 0;
        for (std::size_t index = 0; index < node.components().size(); ++index) {
            const auto* missing = dynamic_cast<const MissingComponent*>(node.components()[index].get());
            const TypeInfo* type = missing ? registry.find(missing->missingType()) : nullptr;
            if (!type || type->kind() != TypeKind::Component || !type->creatable()
                || !canAttach(registry, *type, node)) {
                continue;
            }
            const Json data = missing->data();
            const ComponentId id = missing->id();
            node.removeComponent(*node.components()[index]);
            Component& component = node.insertComponent(index, type->createComponent());
            assignId(component, id);
            readProperties(*type, component, data);
            ++revived;
        }
        if (revived > 0) {
            addMissingRequirements(node);
        }
        for (const auto& child : node.children()) {
            revived += revive(*child);
        }

        return revived;
    }

    // ID компонента из файла. Без него (файл до ID компонентов) или с ID,
    // уже занятым на этом узле (файл правили руками), остаётся новый.
    void readComponentId(const Node& node, Component& component, const Json& json) const
    {
        if (freshIds) {
            return;
        }
        const auto id = json.contains(kId) ? idFromJson(json[kId]) : std::nullopt;
        if (!id || *id == 0) {
            return;
        }
        const auto taken = std::ranges::any_of(node.components(), [&](const auto& other) {
            return other.get() != &component && other->id() == *id;
        });
        if (taken) {
            log::warn("Scene: component ID repeats on node '{}', the component gets a new one", node.name());

            return;
        }
        assignId(component, *id);
    }

    // Файл правили руками или правила типа поменялись: обязательного компонента
    // в файле нет. Добавляем его со значениями по умолчанию, чтобы правила
    // выполнялись для любой загруженной сцены.
    void addMissingRequirements(Node& node) const
    {
        std::vector<const TypeInfo*> present;
        for (const auto& component : node.components()) {
            if (const TypeInfo* type = registry.find(component->typeName())) {
                present.push_back(type);
            }
        }
        for (const TypeInfo* type : present) {
            const auto missing = missingRequirements(registry, *type, node);
            if (!missing) {
                log::error(
                    "Scene: requirements of component '{}' on node '{}' cannot be met",
                    type->name(),
                    node.name()
                );
                continue;
            }
            for (const TypeInfo* required : *missing) {
                log::warn(
                    "Scene: component '{}' on node '{}' requires '{}', added",
                    type->name(),
                    node.name(),
                    required->name()
                );
                node.addComponent(required->createComponent());
            }
        }
    }
};

SceneSerializer::SceneSerializer(const TypeRegistry& registry)
    : m_registry(registry)
{
}

void SceneSerializer::assignId(Node& node, NodeId id)
{
    node.setId(id);
}

void SceneSerializer::assignId(Component& component, ComponentId id)
{
    component.setId(id);
}

Json SceneSerializer::Format::write(const Scene& scene) const
{
    Json json;
    kFormat.stamp(json);
    json[kRoot] = scene.root() ? nodeToJson(registry, *scene.root()) : Json(nullptr);

    return json;
}

bool SceneSerializer::Format::readScene(Scene& scene, Json json) const
{
    if (!json.is_object()) {
        log::error("Scene: file is not a valid JSON object");

        return false;
    }
    if (!kFormat.migrate(json)) {
        return false;
    }

    // Дерево собираем отдельно и отдаём сцене, только если всё прочиталось.
    std::unique_ptr<Node> root;
    if (const auto it = json.find(kRoot); it != json.end() && !it->is_null()) {
        root = readNode(*it);
        if (!root) {
            return false;
        }
    }

    scene.setRoot(std::move(root));

    return true;
}

std::string SceneSerializer::toJson(const Scene& scene) const
{
    return Format{m_registry}.write(scene).dump(kJsonIndent);
}

bool SceneSerializer::fromJson(Scene& scene, std::string_view text) const
{
    Json json = Json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded()) {
        log::error("Scene: file is not a valid JSON object");

        return false;
    }

    const PrefabSources sources = PrefabSources::of(scene.services());

    return Format{m_registry, false, &sources}.readScene(scene, std::move(json));
}

bool SceneSerializer::save(const Scene& scene, const std::filesystem::path& file) const
{
    if (!writeJsonFile(file, Format{m_registry}.write(scene))) {
        log::error("Scene: cannot write '{}'", toUtf8(file));

        return false;
    }

    return true;
}

std::size_t SceneSerializer::reviveMissing(Scene& scene) const
{
    assert(!scene.inFrame() && "reviveMissing: not during a frame of the scene");

    return scene.root() ? Format{m_registry}.revive(*scene.root()) : 0;
}

bool SceneSerializer::load(Scene& scene, const std::filesystem::path& file) const
{
    auto json = std::filesystem::exists(file) ? readJsonFile(file) : std::nullopt;
    if (!json) {
        log::error("Scene: cannot open '{}'", toUtf8(file));

        return false;
    }

    const PrefabSources sources = PrefabSources::of(scene.services());

    return Format{m_registry, false, &sources}.readScene(scene, std::move(*json));
}

Json SceneBranches::write(const TypeRegistry& registry, const Node& node)
{
    return nodeToJson(registry, node);
}

std::unique_ptr<Node> SceneBranches::read(
    const TypeRegistry& registry,
    const Json& node,
    BranchIds ids,
    const PrefabSources* sources
)
{
    return SceneSerializer::Format{registry, ids == BranchIds::Fresh, sources}.readNode(node);
}

std::unique_ptr<Node> SceneBranches::instantiate(
    const TypeRegistry& registry,
    ResourceId prefab,
    const PrefabSources& sources
)
{
    const Json record{{scene_keys::kPrefab, idToJson(prefab)}, {kId, idToJson(newObjectId())}};
    auto node = read(registry, record, BranchIds::FromFile, &sources);
    const PrefabInstance* instance = node ? node->prefabInstance() : nullptr;

    return instance && instance->base ? std::move(node) : nullptr;
}

std::optional<Json> SceneBranches::rootOfFile(Json file)
{
    if (!file.is_object() || !kFormat.migrate(file)) {
        return std::nullopt;
    }
    const auto it = file.find(kRoot);

    return it != file.end() ? std::optional(std::move(*it)) : std::optional(Json(nullptr));
}

} // namespace yqlora
