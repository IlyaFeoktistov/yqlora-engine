#include "EngineRegistry.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/JsonFile.hpp>
#include <yqlora/core/JsonId.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/PrefabEditing.hpp>
#include <yqlora/editor/ProjectFiles.hpp>
#include <yqlora/editor/ProjectSetup.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/ObjectId.hpp>
#include <yqlora/scene/PrefabInstance.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneBranches.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scene/Timer.hpp>
#include <yqlora/scripting/GameModule.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

using namespace yqlora;
using namespace yqlora::editor;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

// Ссылка на узел: её перевод, когда ветка становится префабом.
struct Pointer final : Component {
    YQLORA_TYPE(Pointer, Component)

    NodeRef target;
};

struct Registry : test::EngineRegistry {
    Registry()
    {
        registerComponent<Pointer>().property("target", &Pointer::target);
    }
};

Node& childNamed(const Node& parent, std::string_view name)
{
    Node* found = parent.findChild(name);
    REQUIRE(found);

    return *found;
}

struct Fixture {
    Fixture()
    {
        std::string error;
        const auto created = createProject(test::freshDirectory("prefab_editor"), "Game", serializer, error);
        REQUIRE(created);
        directory = created->directory();
        REQUIRE(session.open(*Project::open(directory, registry)));
        door = writeDoor();
    }

    // Дверь: створка с мешем и ручка.
    ResourceId writeDoor()
    {
        Node3D root{"Door"};
        root.addChild<Node3D>("Leaf").addComponent<MeshRenderer>().setMesh(ResourceRef{builtin::kCubeMesh});
        root.addChild<Node3D>("Handle");
        ProjectFiles files(*session.project());
        const auto path = files.createPrefab({}, "Door", SceneBranches::write(registry, root));
        REQUIRE(path);

        return session.project()->resources().id(*path);
    }

    const Property& nameProperty() const
    {
        const auto properties = registry.properties(*registry.find("Node"));
        const auto it = std::ranges::find(properties, "name", &Property::name);
        REQUIRE(it != properties.end());

        return **it;
    }

    // Переименование узла правкой с отменой.
    void rename(Node& node, const std::string& name)
    {
        REQUIRE(session.editing().setProperty(node, nameProperty(), PropertyValue{name}));
        session.finishEdit();
    }

    Node& node(NodeId id)
    {
        Node* found = session.findNode(id);
        REQUIRE(found);

        return *found;
    }

    Node& drop(ResourceId prefab)
    {
        Node* added = session.editing().addResource(prefab, nullptr);
        REQUIRE(added);
        REQUIRE(added->prefabInstance());

        return *added;
    }

    // Файл префаба переписан мимо редактора.
    void rewritePrefab(const fs::path& relative, const std::function<void(Json& root)>& change)
    {
        const fs::path file = directory / relative;
        const auto before = fs::last_write_time(file);
        Json json = *readJsonFile(file);
        change(json["root"]);
        REQUIRE(writeJsonFile(file, json));
        test::markRewritten(file, before);
        session.refreshResources();
    }

    Json doorFile() const
    {
        return *readJsonFile(directory / "Door.yprefab");
    }

    Registry registry;
    SceneSerializer serializer{registry};
    Input input;
    EngineServices services{input, registry};
    GameModule gameModule{registry};
    EditorSession session{services, serializer, gameModule};
    fs::path directory;
    ResourceId door = 0;
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "A dropped prefab is an instance; a prefab is not dropped into itself")
{
    Node& instance = drop(door);
    CHECK(instance.name() == "Door");
    CHECK(childNamed(instance, "Leaf").prefabLink().instance == instance.prefabInstance()->id);
    REQUIRE(session.undo());
    CHECK(session.findNode(instance.id()) == nullptr);

    // Дом с дверью внутри.
    ProjectFiles files(*session.project());
    const Json record{{"prefab", idToJson(door)}, {"id", idToJson(newObjectId())}};
    const auto housePath = files.createPrefab({}, "House", Json{{"type", "Node3D"}, {"children", {record}}});
    REQUIRE(housePath);
    const ResourceId house = session.project()->resources().id(*housePath);

    CHECK_FALSE(session.editingPrefab());
    CHECK(session.prefabLighting() == nullptr);
    REQUIRE(session.openScene("Door.yprefab"));
    // Префаб без своего неба и солнца освещается светом предпросмотра.
    CHECK(session.editingPrefab());
    CHECK(session.prefabLighting());
    CHECK(session.editing().addResource(door, nullptr) == nullptr);
    CHECK(session.editing().addResource(house, nullptr) == nullptr);
    CHECK_FALSE(session.prefabEditing().canInstantiate(house));

    REQUIRE(session.openScene(*housePath));
    CHECK(session.editing().addResource(door, nullptr));
}

TEST_CASE_FIXTURE(Fixture, "Revert: a property, a component, the whole instance, each in one undo step")
{
    const NodeId root = drop(door).id();
    const NodeId leaf = childNamed(node(root), "Leaf").id();
    rename(node(leaf), "Panel");
    CHECK(InstanceOverrides(node(root), registry).property(node(leaf), nullptr, "name"));

    REQUIRE(session.prefabEditing().revertProperty(node(leaf), nullptr, nameProperty()));
    CHECK(node(leaf).name() == "Leaf");
    CHECK_FALSE(InstanceOverrides(node(root), registry).any());
    REQUIRE(session.undo());
    CHECK(node(leaf).name() == "Panel");
    REQUIRE(session.redo());
    CHECK(node(leaf).name() == "Leaf");

    // Добавленный компонент возвращается тем, что его нет.
    Component* timer = session.editing().addComponent(node(leaf), *registry.find("Timer"));
    REQUIRE(timer);
    CHECK(InstanceOverrides(node(root), registry).addedComponent(node(leaf), *timer));
    REQUIRE(session.prefabEditing().revertComponent(node(leaf), *timer));
    CHECK(node(leaf).getComponent<Timer>() == nullptr);

    // Весь экземпляр: своё имя корня остаётся.
    rename(node(root), "FrontDoor");
    rename(node(leaf), "Panel");
    REQUIRE(session.prefabEditing().revertInstance(node(root)));
    CHECK(node(root).name() == "FrontDoor");
    CHECK(node(leaf).name() == "Leaf");
    REQUIRE(session.undo());
    CHECK(node(leaf).name() == "Panel");
}

TEST_CASE_FIXTURE(Fixture, "Apply to prefab changes the file and the other instances; undo restores both")
{
    const NodeId first = drop(door).id();
    const NodeId second = drop(door).id();
    const NodeId firstLeaf = childNamed(node(first), "Leaf").id();
    const NodeId secondLeaf = childNamed(node(second), "Leaf").id();
    rename(node(firstLeaf), "Panel");
    REQUIRE(session.prefabEditing().canApply(node(first)));

    REQUIRE(session.prefabEditing().applyProperty(node(firstLeaf), nullptr, nameProperty()));
    CHECK(doorFile()["root"]["children"][0]["properties"]["name"] == "Panel");
    CHECK(node(secondLeaf).name() == "Panel");
    CHECK(node(firstLeaf).name() == "Panel");
    CHECK_FALSE(InstanceOverrides(node(first), registry).any());

    REQUIRE(session.undo());
    CHECK(doorFile()["root"]["children"][0]["properties"]["name"] == "Leaf");
    CHECK(node(secondLeaf).name() == "Leaf");
    CHECK(node(firstLeaf).name() == "Panel");
    REQUIRE(session.redo());
    CHECK(node(secondLeaf).name() == "Panel");

    // Добавленный узел - тоже в префаб, у второго экземпляра он появляется.
    REQUIRE(session.editing().createNode(*registry.find("Node3D"), &node(first)));
    REQUIRE(session.prefabEditing().applyInstance(node(first)));
    CHECK(node(second).children().size() == 3);
    CHECK_FALSE(InstanceOverrides(node(first), registry).any());
    CHECK(node(first).children().size() == 3);

    // Модель - только для чтения.
    Node& cube = drop(builtin::kCubeMesh);
    CHECK_FALSE(session.prefabEditing().canApply(cube));
}

TEST_CASE_FIXTURE(Fixture, "Applying an added instance of the prefab itself is refused")
{
    Node& outer = drop(door);
    REQUIRE(session.editing().addResource(door, &outer));
    const Json before = doorFile();
    CHECK_FALSE(session.prefabEditing().applyInstance(outer));
    CHECK(doorFile() == before);
}

TEST_CASE_FIXTURE(Fixture, "Undo after the prefab changed finds its nodes")
{
    const NodeId root = drop(door).id();
    const NodeId leaf = childNamed(node(root), "Leaf").id();
    rename(node(leaf), "Panel");

    rewritePrefab("Door.yprefab", [](Json& prefab) {
        Json knob = prefab["children"][1];
        knob["id"] = idToJson(newObjectId());
        knob["properties"]["name"] = "Knob";
        prefab["children"].push_back(knob);
    });
    CHECK(node(root).findChild("Knob"));
    CHECK(node(leaf).name() == "Panel");
    REQUIRE(session.undo());
    CHECK(node(leaf).name() == "Leaf");
    CHECK(node(root).findChild("Knob"));
}

TEST_CASE_FIXTURE(Fixture, "Unused overrides are counted and removed")
{
    const NodeId root = drop(door).id();
    rename(childNamed(node(root), "Handle"), "Grip");
    rewritePrefab("Door.yprefab", [](Json& prefab) {
        prefab["children"].erase(1);
    });
    CHECK(InstanceOverrides(node(root), registry).unused() == 1);
    REQUIRE(session.prefabEditing().removeUnused(node(root)));
    CHECK(InstanceOverrides(node(root), registry).unused() == 0);
    REQUIRE(session.undo());
    CHECK(InstanceOverrides(node(root), registry).unused() == 1);
}

TEST_CASE_FIXTURE(Fixture, "Unpack keeps the IDs in one undo step")
{
    Node& instance = drop(door);
    const NodeId root = instance.id();
    const NodeId leaf = childNamed(instance, "Leaf").id();
    REQUIRE(session.prefabEditing().unpack(instance));
    CHECK(node(root).prefabInstance() == nullptr);
    CHECK_FALSE(node(leaf).prefabLink());
    REQUIRE(session.undo());
    CHECK(node(root).prefabInstance());
    CHECK(node(leaf).prefabLink());
}

TEST_CASE_FIXTURE(Fixture, "A branch saved as a prefab becomes its instance, references follow it")
{
    Node& lamp = *session.editing().createNode(*registry.find("Node3D"), nullptr);
    rename(lamp, "Lamp");
    const NodeId lampId = lamp.id();
    Node* bulb = session.editing().createNode(*registry.find("Node3D"), &node(lampId));
    REQUIRE(bulb);
    const NodeId bulbId = bulb->id();
    Node* watcher = session.editing().createNode(*registry.find("Node3D"), nullptr);
    REQUIRE(watcher);
    const NodeId watcherId = watcher->id();
    watcher->addComponent<Pointer>().target = NodeRef{bulbId};

    Node* instance = session.prefabEditing().saveAsPrefab(node(lampId), {}, "Lamp");
    REQUIRE(instance);
    REQUIRE(fs::exists(directory / "Lamp.yprefab"));
    REQUIRE(instance->prefabInstance());
    CHECK(instance->name() == "Lamp");
    CHECK(session.selection().selected() == instance->id());
    const NodeId newBulb = instance->children().front()->id();
    CHECK(newBulb == deriveId(instance->prefabInstance()->id, bulbId));
    CHECK(node(watcherId).getComponent<Pointer>()->target.id == newBulb);
    CHECK(session.findNode(lampId) == nullptr);

    REQUIRE(session.undo());
    CHECK(node(lampId).prefabInstance() == nullptr);
    CHECK(node(watcherId).getComponent<Pointer>()->target.id == bulbId);
    // Имя занято - не создаётся.
    CHECK(session.prefabEditing().saveAsPrefab(node(lampId), {}, "Lamp") == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "A variant follows its base prefab")
{
    ProjectFiles files(*session.project());
    const auto path = files.createVariant({}, "BigDoor", door);
    REQUIRE(path);
    const ResourceId variant = session.project()->resources().id(*path);
    const NodeId root = drop(variant).id();
    const NodeId leaf = childNamed(node(root), "Leaf").id();

    rewritePrefab("Door.yprefab", [](Json& prefab) {
        prefab["children"][0]["properties"]["name"] = "Wing";
    });
    CHECK(node(leaf).name() == "Wing");
}
