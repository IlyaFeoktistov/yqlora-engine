#include "EngineRegistry.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/JsonFile.hpp>
#include <yqlora/core/JsonId.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/render/PointLight3D.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/ObjectId.hpp>
#include <yqlora/scene/PrefabExpansion.hpp>
#include <yqlora/scene/PrefabInstance.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneBranches.hpp>
#include <yqlora/scene/SceneSerializer.hpp>

#include <filesystem>
#include <optional>
#include <string>

using namespace yqlora;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

// Компонент со ссылками на узел и компонент: их перевод между пространствами ID.
struct Pointer final : Component {
    YQLORA_TYPE(Pointer, Component)

    NodeRef target;
    ComponentRef<> part;
};

constexpr NodeId kFirst = 0x1111;
constexpr NodeId kSecond = 0x2222;

Json recordOf(ResourceId prefab, NodeId instance)
{
    return {{"prefab", idToJson(prefab)}, {"id", idToJson(instance)}};
}

Node& childNamed(const Node& parent, std::string_view name)
{
    Node* found = parent.findChild(name);
    REQUIRE(found);

    return *found;
}

struct Fixture {
    Fixture()
    {
        registry.registerComponent<Pointer>().property("target", &Pointer::target).property("part", &Pointer::part);
        REQUIRE(Project::create(directory, "Game", registry));
        project = Project::open(directory, registry);
        REQUIRE(project);
        services.setProject(&*project);
    }

    // Дверь: корень, створка с мешем, ручка, которая ссылается на створку и
    // её меш, и рама.
    void makeDoor(Scene& scene) const
    {
        auto& root = scene.createRoot<Node3D>("Door");
        auto& leaf = root.addChild<Node3D>("Leaf");
        auto& renderer = leaf.addComponent<MeshRenderer>();
        renderer.setMesh(ResourceRef{builtin::kCubeMesh});
        auto& handle = root.addChild<Node3D>("Handle");
        auto& pointer = handle.addComponent<Pointer>();
        pointer.target = NodeRef{leaf.id()};
        pointer.part = {leaf.id(), renderer.id()};
        root.addChild<Node3D>("Frame");
    }

    // Пишет файл префаба сцены scene с записями экземпляров records под
    // корнем и возвращает ID файла; перезапись перечитывается кэшем.
    ResourceId writePrefab(const std::string& name, const Scene& scene, const std::vector<Json>& records = {})
    {
        Json file = Json::parse(serializer.toJson(scene));
        for (const Json& record : records) {
            file["root"]["children"].push_back(record);
        }
        REQUIRE(writeJsonFile(directory / name, file));
        ResourceId id = project->resources().id(name);
        if (id == 0) {
            id = project->resources().add(name);
        } else {
            REQUIRE(services.resources().reload(id));
        }

        return id;
    }

    ResourceId writeDoor(Scene& door)
    {
        makeDoor(door);

        return writePrefab("Door.yprefab", door);
    }

    // Уровень: корень Level и под ним записи records.
    std::string level(const std::vector<Json>& records) const
    {
        Scene scene{services};
        scene.createRoot<Node3D>("Level");
        Json file = Json::parse(serializer.toJson(scene));
        for (const Json& record : records) {
            file["root"]["children"].push_back(record);
        }

        return file.dump();
    }

    // Сохраняет scene и загружает в loaded.
    void reload(const Scene& scene, Scene& loaded) const
    {
        REQUIRE(serializer.fromJson(loaded, serializer.toJson(scene)));
    }

    // Запись экземпляра в сохранённой сцене, по номеру среди детей корня.
    Json savedChild(const Scene& scene, std::size_t index) const
    {
        return Json::parse(serializer.toJson(scene))["root"]["children"][index];
    }

    fs::path directory = test::freshDirectory("prefab_instances");
    test::EngineRegistry registry;
    std::optional<Project> project;
    Input input;
    EngineServices services{input, registry};
    SceneSerializer serializer{registry};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "An instance is kept as a reference and expands into nodes with derived IDs")
{
    Scene door{services};
    const ResourceId prefab = writeDoor(door);
    const Node& source = *door.root();

    Scene scene{services};
    REQUIRE(serializer.fromJson(scene, level({recordOf(prefab, kFirst)})));
    const Node& root = *scene.root()->children().front();
    CHECK(root.name() == "Door");
    CHECK(root.id() == deriveId(kFirst, source.id()));
    CHECK(root.prefabLink() == PrefabLink{kFirst, source.id()});
    REQUIRE(root.prefabInstance());
    CHECK(root.prefabInstance()->prefab == prefab);

    const Node& leaf = childNamed(root, "Leaf");
    const Node& sourceLeaf = childNamed(source, "Leaf");
    CHECK(leaf.id() == deriveId(kFirst, sourceLeaf.id()));
    CHECK(leaf.prefabLink() == PrefabLink{kFirst, sourceLeaf.id()});
    const auto* renderer = leaf.getComponent<MeshRenderer>();
    REQUIRE(renderer);
    CHECK(renderer->id() == deriveId(kFirst, sourceLeaf.getComponent<MeshRenderer>()->id()));

    // Ссылки внутри префаба ведут на узлы этого экземпляра.
    const auto* pointer = childNamed(root, "Handle").getComponent<Pointer>();
    REQUIRE(pointer);
    CHECK(pointer->target.id == leaf.id());
    CHECK(pointer->part == ComponentRef<>{leaf.id(), renderer->id()});

    // В файле - ссылка и только свои значения корня, без узлов префаба.
    const Json saved = savedChild(scene, 0);
    CHECK(idFromJson(saved["prefab"]) == prefab);
    CHECK(idFromJson(saved["id"]) == kFirst);
    CHECK_FALSE(saved.contains("children"));
    for (const Json& entry : saved["overrides"]) {
        CHECK(idFromJson(entry["node"]) == source.id());
    }

    Scene again{services};
    reload(scene, again);
    CHECK(childNamed(*again.root()->children().front(), "Leaf").id() == leaf.id());
    CHECK(savedChild(again, 0) == saved);
}

TEST_CASE_FIXTURE(Fixture, "Every kind of override goes there and back")
{
    Scene door{services};
    const ResourceId prefab = writeDoor(door);

    Scene scene{services};
    REQUIRE(serializer.fromJson(scene, level({recordOf(prefab, kFirst)})));
    auto& root = static_cast<Node3D&>(*scene.root()->children().front());
    root.setName("FrontDoor");
    root.transform().setPosition(Vec3(2.0f, 0.0f, 0.0f));
    Node& leaf = childNamed(root, "Leaf");
    leaf.setName("Panel");
    Node& handle = childNamed(root, "Handle");
    handle.removeComponent(*handle.getComponent<Pointer>());
    auto& light = handle.addChild<PointLight3D>("Glow");
    light.setRange(3.0f);
    auto& added = leaf.addComponent<Pointer>();
    added.target = NodeRef{light.id()};
    root.removeChild(childNamed(root, "Frame"));

    Scene loaded{services};
    reload(scene, loaded);
    const auto& copy = static_cast<const Node3D&>(*loaded.root()->children().front());
    CHECK(copy.name() == "FrontDoor");
    CHECK(copy.transform().position() == Vec3(2.0f, 0.0f, 0.0f));
    CHECK(copy.findChild("Frame") == nullptr);
    const Node& panel = childNamed(copy, "Panel");
    CHECK(panel.id() == leaf.id());
    const Node& copiedHandle = childNamed(copy, "Handle");
    CHECK(copiedHandle.getComponent<Pointer>() == nullptr);
    const auto* glow = dynamic_cast<const PointLight3D*>(copiedHandle.findChild("Glow"));
    REQUIRE(glow);
    CHECK(glow->id() == light.id());
    CHECK(glow->range() == doctest::Approx(3.0f));
    CHECK_FALSE(glow->prefabLink());
    const auto* copiedAdded = panel.getComponent<Pointer>();
    REQUIRE(copiedAdded);
    CHECK(copiedAdded->id() == added.id());
    CHECK(copiedAdded->target.id == light.id());

    // Повторное сохранение даёт ту же запись: переопределения устойчивы.
    CHECK(savedChild(loaded, 0) == savedChild(scene, 0));
}

TEST_CASE_FIXTURE(Fixture, "A change of the prefab reaches instances that do not override it")
{
    Scene door{services};
    const ResourceId prefab = writeDoor(door);

    Scene scene{services};
    REQUIRE(serializer.fromJson(scene, level({recordOf(prefab, kFirst), recordOf(prefab, kSecond)})));
    Node& first = *scene.root()->children()[0];
    Node& firstLeaf = childNamed(first, "Leaf");
    firstLeaf.setName("Panel");
    // Ссылка снаружи внутрь экземпляра.
    auto& watcher = scene.root()->addChild<Node3D>("Watcher");
    watcher.addComponent<Pointer>().target = NodeRef{firstLeaf.id()};
    const std::string saved = serializer.toJson(scene);

    childNamed(*door.root(), "Leaf").setName("Wing");
    door.root()->addChild<Node3D>("Knob");
    writePrefab("Door.yprefab", door);

    Scene loaded{services};
    REQUIRE(serializer.fromJson(loaded, saved));
    const Node& copyFirst = *loaded.root()->children()[0];
    const Node& copySecond = *loaded.root()->children()[1];
    CHECK(copyFirst.findChild("Panel"));
    CHECK(copyFirst.findChild("Wing") == nullptr);
    CHECK(copySecond.findChild("Wing"));
    CHECK(copyFirst.findChild("Knob"));
    CHECK(copySecond.findChild("Knob"));
    const auto* pointer = childNamed(*loaded.root(), "Watcher").getComponent<Pointer>();
    REQUIRE(pointer);
    CHECK(pointer->target.id == childNamed(copyFirst, "Panel").id());
}

TEST_CASE_FIXTURE(Fixture, "An override whose target left the prefab stays in the file unused")
{
    Scene door{services};
    const ResourceId prefab = writeDoor(door);
    const NodeId frame = childNamed(*door.root(), "Frame").id();

    Scene scene{services};
    REQUIRE(serializer.fromJson(scene, level({recordOf(prefab, kFirst)})));
    childNamed(*scene.root()->children().front(), "Frame").setName("Arch");
    const std::string saved = serializer.toJson(scene);

    door.root()->removeChild(childNamed(*door.root(), "Frame"));
    writePrefab("Door.yprefab", door);

    Scene loaded{services};
    REQUIRE(serializer.fromJson(loaded, saved));
    const Node& root = *loaded.root()->children().front();
    CHECK(root.findChild("Arch") == nullptr);
    CHECK(root.findChild("Frame") == nullptr);
    REQUIRE(root.prefabInstance());
    CHECK(root.prefabInstance()->unused["overrides"].size() == 1);

    const Json record = savedChild(loaded, 0);
    bool kept = false;
    for (const Json& entry : record["overrides"]) {
        kept = kept || (idFromJson(entry["node"]) == frame && entry["value"] == "Arch");
    }
    CHECK(kept);
}

TEST_CASE_FIXTURE(Fixture, "Nested prefabs: overrides reach their nodes, inner changes reach outer instances")
{
    Scene door{services};
    const ResourceId doorId = writeDoor(door);
    constexpr NodeId kNested = 0xD1;
    Scene house{services};
    house.createRoot<Node3D>("House");
    const ResourceId houseId = writePrefab("House.yprefab", house, {recordOf(doorId, kNested)});

    Scene scene{services};
    REQUIRE(serializer.fromJson(scene, level({recordOf(houseId, kFirst)})));
    const Node& outer = *scene.root()->children().front();
    Node& leaf = childNamed(childNamed(outer, "Door"), "Leaf");
    const NodeId sourceLeaf = childNamed(*door.root(), "Leaf").id();
    CHECK(leaf.id() == deriveId(kFirst, deriveId(kNested, sourceLeaf)));
    CHECK(leaf.prefabLink() == PrefabLink{kFirst, deriveId(kNested, sourceLeaf)});
    // Ссылки внутреннего префаба ведут в этот экземпляр.
    CHECK(childNamed(childNamed(outer, "Door"), "Handle").getComponent<Pointer>()->target.id == leaf.id());
    leaf.setName("Inner");
    const std::string saved = serializer.toJson(scene);

    door.root()->addChild<Node3D>("Knob");
    writePrefab("Door.yprefab", door);

    Scene loaded{services};
    REQUIRE(serializer.fromJson(loaded, saved));
    const Node& copy = childNamed(*loaded.root()->children().front(), "Door");
    CHECK(childNamed(copy, "Inner").id() == leaf.id());
    CHECK(copy.findChild("Knob"));
}

TEST_CASE_FIXTURE(Fixture, "A prefab that contains itself through another is an error, not a hang")
{
    Scene first{services};
    first.createRoot<Node3D>("First");
    const ResourceId firstId = writePrefab("First.yprefab", first);
    Scene second{services};
    second.createRoot<Node3D>("Second");
    const ResourceId secondId = writePrefab("Second.yprefab", second, {recordOf(firstId, 0xA)});
    writePrefab("First.yprefab", first, {recordOf(secondId, 0xB)});

    Scene scene{services};
    REQUIRE(serializer.fromJson(scene, level({recordOf(firstId, kFirst)})));
    const Node& root = *scene.root()->children().front();
    CHECK(root.name() == "First");
    const Node& inner = childNamed(root, "Second");
    CHECK(inner.children().empty());
}

TEST_CASE_FIXTURE(Fixture, "An instance of a missing prefab keeps its record as is")
{
    Json record = recordOf(0xDEAD, kFirst);
    record["overrides"] = Json::array({{{"node", idToJson(0x5)}, {"property", "name"}, {"value", "Gate"}}});
    record["removed"] = Json::array({idToJson(0x6)});

    Scene scene{services};
    REQUIRE(serializer.fromJson(scene, level({record})));
    const Node& placeholder = *scene.root()->children().front();
    CHECK(placeholder.name() == "Gate");
    CHECK(savedChild(scene, 0) == record);
}

TEST_CASE_FIXTURE(Fixture, "A copy of an instance gets a new instance ID and its nodes follow")
{
    Scene door{services};
    const ResourceId prefab = writeDoor(door);
    Scene scene{services};
    REQUIRE(serializer.fromJson(scene, level({recordOf(prefab, kFirst)})));
    const Node& original = *scene.root()->children().front();

    const Json branch = SceneBranches::write(registry, original);
    const PrefabSources sources = PrefabSources::of(services);
    const auto copy = SceneBranches::read(registry, branch, BranchIds::Fresh, &sources);
    REQUIRE(copy);
    REQUIRE(copy->prefabInstance());
    const NodeId instance = copy->prefabInstance()->id;
    CHECK(instance != kFirst);
    CHECK(copy->id() == deriveId(instance, door.root()->id()));
    const Node& leaf = childNamed(*copy, "Leaf");
    CHECK(childNamed(*copy, "Handle").getComponent<Pointer>()->target.id == leaf.id());
}

TEST_CASE_FIXTURE(Fixture, "A scene is not a prefab: its record stays a placeholder")
{
    Scene levelScene{services};
    levelScene.createRoot<Node3D>("Room");
    REQUIRE(serializer.save(levelScene, directory / "Room.yscene"));
    const ResourceId room = project->resources().add("Room.yscene");

    Scene scene{services};
    REQUIRE(serializer.fromJson(scene, level({recordOf(room, kFirst)})));
    const Node& placeholder = *scene.root()->children().front();
    REQUIRE(placeholder.prefabInstance());
    CHECK(placeholder.prefabInstance()->base == nullptr);
}
