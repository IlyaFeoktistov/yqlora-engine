#include "EngineRegistry.hpp"
#include "GltfBuilder.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/ModelInspection.hpp>
#include <yqlora/editor/Picking.hpp>
#include <yqlora/editor/ProjectSetup.hpp>
#include <yqlora/editor/ResourceChoices.hpp>
#include <yqlora/editor/ResourceInspection.hpp>
#include <yqlora/editor/ResourceInstance.hpp>
#include <yqlora/editor/ResourceLabel.hpp>
#include <yqlora/editor/TextureInspection.hpp>
#include <yqlora/i18n/Translate.hpp>
#include <yqlora/import/ModelData.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/project/GeneralSettings.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/render/BuiltinMeshes.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/render/TextureData.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/resource/ResourceLoader.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scripting/GameModule.hpp>
#include <yqlora/shader/BuiltinShaders.hpp>
#include <yqlora/shader/ShaderData.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

using namespace yqlora;
using namespace yqlora::editor;
using namespace yqlora::test;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

// Дом: две двери с одним мешем и рама с другим, под общим узлом.
GltfBuilder house()
{
    GltfBuilder gltf;
    const int positions = gltf.positions({0, 0, 0, 1, 0, 0, 0, 1, 0});
    const Json primitive{{"attributes", {{"POSITION", positions}}}};
    gltf.json["meshes"] = {
        {{"name", "Door"}, {"primitives", {primitive}}},
        {{"name", "Frame"}, {"primitives", {primitive}}},
    };
    gltf.json["nodes"] = {
        {{"name", "House"}, {"children", {1, 2, 3}}, {"translation", {0.0, 1.0, 0.0}}},
        {{"name", "LeftDoor"}, {"mesh", 0}, {"translation", {-1.0, 0.0, 0.0}}},
        {{"name", "RightDoor"}, {"mesh", 0}, {"translation", {1.0, 0.0, 0.0}}},
        {{"mesh", 1}, {"scale", {2.0, 2.0, 2.0}}},
    };
    gltf.json["scenes"] = {{{"nodes", {0}}}};

    return gltf;
}

// Открытый в сессии проект с моделью models/house.glb.
struct Fixture {
    Fixture()
    {
        const auto location = test::freshDirectory("resource_editor");
        std::string error;
        const auto created = createProject(location, "Game", serializer, error);
        REQUIRE(created);
        directory = created->directory();
        fs::create_directories(directory / "models");
        house().writeGlb(directory / "models" / "house.glb");
        REQUIRE(session.open(*Project::open(directory, registry)));
        model = session.project()->resources().id("models/house.glb");
        REQUIRE(model != 0);
    }

    ResourceId mesh(const std::string& key)
    {
        REQUIRE(session.resources().find<ModelData>(model));

        return session.project()->resources().subresources(model).at(key);
    }

    test::EngineRegistry registry;
    SceneSerializer serializer{registry};
    Input input;
    EngineServices services{input, registry};
    GameModule gameModule{registry};
    EditorSession session{services, serializer, gameModule};
    fs::path directory;
    ResourceId model = 0;
};

const MeshRenderer* rendererOf(const Node& node)
{
    return node.getComponent<MeshRenderer>();
}

} // namespace

TEST_CASE_FIXTURE(Fixture, "instantiateResource: a model becomes its node tree under a node named after the file")
{
    const auto branch = instantiateResource(model, session.resources(), session.project()->resources());
    REQUIRE(branch);
    CHECK(branch->name() == "house");
    REQUIRE(branch->children().size() == 1);
    const auto& houseNode = static_cast<const Node3D&>(*branch->children()[0]);
    CHECK(houseNode.name() == "House");
    CHECK(houseNode.transform().position() == Vec3(0.0f, 1.0f, 0.0f));
    REQUIRE(houseNode.children().size() == 3);

    const Node& left = *houseNode.children()[0];
    const Node& right = *houseNode.children()[1];
    const auto& frame = static_cast<const Node3D&>(*houseNode.children()[2]);
    // Один меш у двух дверей - одна ссылка на один ресурс.
    REQUIRE(rendererOf(left));
    CHECK(rendererOf(left)->mesh().id == mesh("mesh:Door"));
    CHECK(rendererOf(right)->mesh().id == rendererOf(left)->mesh().id);
    CHECK(frame.name() == "Node3D");
    CHECK(frame.transform().scale() == Vec3(2.0f));
    CHECK(rendererOf(frame)->mesh().id == mesh("mesh:Frame"));
    CHECK(rendererOf(houseNode) == nullptr);

    // Меш - один узел с его именем.
    const auto single = instantiateResource(mesh("mesh:Frame"), session.resources(), session.project()->resources());
    REQUIRE(single);
    CHECK(single->name() == "Frame");
    CHECK(rendererOf(*single)->mesh().id == mesh("mesh:Frame"));

    // Сцена - не модель и не меш.
    const ResourceId scene = session.project()->settings().get<GeneralSettings>().startupScene.id;
    REQUIRE(scene != 0);
    CHECK(instantiateResource(scene, session.resources(), session.project()->resources()) == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "SceneEditing: a model goes into the scene in one undo step, at the placement point")
{
    Node* root = session.scene().root();
    REQUIRE(root);
    const std::size_t before = root->children().size();
    // В новом проекте уже есть куб.
    const std::size_t meshes = session.scene().components<MeshRenderer>().size();
    Node* added = session.editing().addResource(model, nullptr, Vec3(3.0f, 0.0f, 4.0f));
    REQUIRE(added);
    CHECK(added->parent() == root);
    CHECK(static_cast<Node3D*>(added)->worldPosition() == Vec3(3.0f, 0.0f, 4.0f));
    CHECK(session.selection().selected() == added->id());
    CHECK(session.scene().components<MeshRenderer>().size() == meshes + 3);

    // Второй раз - уникальное имя среди соседей.
    Node* again = session.editing().addResource(model, nullptr);
    REQUIRE(again);
    CHECK(again->name() != added->name());

    REQUIRE(session.undo());
    REQUIRE(session.undo());
    CHECK(root->children().size() == before);
    CHECK(session.scene().components<MeshRenderer>().size() == meshes);
    REQUIRE(session.redo());
    CHECK(session.scene().components<MeshRenderer>().size() == meshes + 3);
}

TEST_CASE_FIXTURE(Fixture, "Scene with a model mesh is saved and reopened with the same references")
{
    Node* added = session.editing().addResource(mesh("mesh:Door"), nullptr);
    REQUIRE(added);
    REQUIRE(session.save());
    const fs::path scene = session.scenePath();
    session.close();

    REQUIRE(session.open(*Project::open(directory, registry)));
    REQUIRE(session.openScene(scene));
    const ResourceId door = mesh("mesh:Door");
    const auto renderers = session.scene().components<MeshRenderer>();
    CHECK(
        std::ranges::count_if(
            renderers,
            [&](const MeshRenderer* renderer) {
                return renderer->mesh().id == door;
            }
        )
        == 1
    );
    CHECK(session.resources().find<MeshData>(door));
}

TEST_CASE_FIXTURE(Fixture, "Re-export keeps references; a deleted file leaves them missing, and they come back")
{
    const ResourceId door = mesh("mesh:Door");
    Node* doorNode = session.editing().addResource(door, nullptr);
    REQUIRE(doorNode);

    // Перевыгрузка: другой порядок мешей, тот же Door.
    GltfBuilder gltf = house();
    std::swap(gltf.json["meshes"][0], gltf.json["meshes"][1]);
    const fs::path file = directory / "models" / "house.glb";
    const auto time = fs::last_write_time(file);
    gltf.writeGlb(file);
    test::markRewritten(file, time);
    session.refreshResources();
    CHECK(mesh("mesh:Door") == door);
    CHECK(session.resources().find<MeshData>(door));

    // Файл удалили мимо редактора: ссылка в сцене остаётся, меша нет.
    fs::remove(file);
    session.refreshResources();
    CHECK(session.resources().find<MeshData>(door) == nullptr);
    CHECK(rendererOf(*doorNode)->mesh().id == door);
    CHECK(resourceLabel(ResourceRef{door}, &session.project()->resources()) == tr("editor.inspector.missing"));

    // Вернули вместе с .meta - всё на месте.
    gltf.writeGlb(file);
    session.refreshResources();
    CHECK(session.resources().find<MeshData>(door));
}

TEST_CASE_FIXTURE(Fixture, "Resource inspection: meshes of a model, import settings applied with a new revision")
{
    const auto inspected = inspectResource(session, model);
    REQUIRE(inspected);
    CHECK(inspected->kind == ModelData::kResourceKind);
    REQUIRE(inspected->status);
    CHECK(inspected->status->state == ResourceState::Ready);
    CHECK(inspectResource(session, mesh("mesh:Door")) == std::nullopt);
    const auto scene = inspectResource(session, session.project()->settings().get<GeneralSettings>().startupScene.id);
    REQUIRE(scene);
    CHECK(scene->kind == SceneSerializer::kResourceKind);
    CHECK_FALSE(scene->status);

    const auto house = inspectModel(session, model);
    REQUIRE(house);
    CHECK(house->nodes == 4);
    REQUIRE(house->meshes.size() == 2);
    CHECK(house->meshes[0].label == "Door");
    CHECK(house->meshes[0].vertices == 3);
    CHECK(house->meshes[0].triangles == 1);
    CHECK(house->import == ModelImport{});
    CHECK_FALSE(inspectModel(session, mesh("mesh:Door")));
    CHECK_FALSE(inspectTexture(session, model));

    const ResourceId door = mesh("mesh:Door");
    const std::uint64_t revision = session.resources().status(door)->revision;
    REQUIRE(applyModelImport(session, model, ModelImport{0.5f}));
    CHECK(session.resources().status(door)->revision > revision);
    CHECK(session.resources().find<MeshData>(door)->vertices[1].position.x == doctest::Approx(0.5f));
    CHECK(inspectModel(session, model)->import == ModelImport{0.5f});
    // Масштаб 1 - значение по умолчанию, в .meta его нет.
    REQUIRE(applyModelImport(session, model, ModelImport{}));
    CHECK(session.project()->resources().importSettings(model) == "{}");
}

TEST_CASE("Selection: keepNodes drops the nodes that are gone and keeps the order")
{
    Selection selection;
    selection.select(3);
    selection.toggle(5);
    selection.toggle(7);
    selection.keepNodes([](NodeId id) {
        return id != 5;
    });
    CHECK(std::vector<NodeId>(selection.nodes().begin(), selection.nodes().end()) == std::vector<NodeId>{3, 7});
    CHECK(selection.selected() == 7);
}

TEST_CASE("Selection: a resource and nodes are not selected together")
{
    Selection selection;
    selection.select(5);
    selection.selectResource(42);
    CHECK(selection.resource() == 42);
    CHECK(selection.empty());
    selection.select(7);
    CHECK(selection.resource() == 0);
    selection.selectResource(42);
    selection.toggle(8);
    CHECK(selection.resource() == 0);
    CHECK(selection.selected() == 8);
}

TEST_CASE("Labels of nested resources and placement in the viewport")
{
    CHECK(subresourceName("mesh:Door#1") == "Door");
    CHECK(subresourceName("mesh:Door#1#1") == "Door#1");
    CHECK(subresourceName("mesh:#2").empty());
    CHECK(subresourceLocalKey("mesh:Door#1") == "Door#1");
    CHECK(subresourceLocalKey("mesh:#2") == "#2");

    // Сверху на землю - точка на земле; вдоль земли - на фиксированном расстоянии.
    CHECK(placementPoint({Vec3(1.0f, 5.0f, 0.0f), Vec3(0.0f, -2.0f, 0.0f)}) == Vec3(1.0f, 0.0f, 0.0f));
    const Vec3 ahead = placementPoint({Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)});
    CHECK(ahead == Vec3(0.0f, 1.0f, -kPlacementDistance));
    const Vec3 far = placementPoint({Vec3(0.0f, 1000.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)});
    CHECK(far.y == doctest::Approx(1000.0f - kPlacementDistance));
}

TEST_CASE_FIXTURE(Fixture, "resourceChoices: built-in resources first, then nested ones by file; files of a kind")
{
    const auto& resources = session.project()->resources();
    const auto meshes = resourceChoices(MeshData::kResourceKind, &resources, session.resources());
    REQUIRE(meshes.size() == builtinMeshes().size() + 2);
    CHECK(meshes[0].builtin);
    CHECK(meshes[0].label == "cube");
    const ResourceChoice& door = meshes[builtinMeshes().size()];
    CHECK_FALSE(door.builtin);
    CHECK(door.label == "Door");
    CHECK(door.group == "models/house.glb");
    CHECK(door.id == mesh("mesh:Door"));

    // Сцены - файлы верхнего уровня, без групп; текстур в проекте нет.
    const auto scenes = resourceChoices(SceneSerializer::kResourceKind, &resources, session.resources());
    REQUIRE(scenes.size() == 1);
    CHECK(scenes[0].group.empty());
    CHECK(resourceChoices(TextureData::kResourceKind, &resources, session.resources()).empty());
    // Встроенные ресурсы любого вида - из одной таблицы.
    const auto shaders = resourceChoices(ShaderData::kResourceKind, nullptr, session.resources());
    REQUIRE(shaders.size() == builtinShaders().size());
    CHECK(shaders[0].label == "Standard");
    // Без проекта - только встроенные.
    CHECK(resourceChoices(MeshData::kResourceKind, nullptr, session.resources()).size() == builtinMeshes().size());
}

TEST_CASE_FIXTURE(Fixture, "resourceForField: a model on a mesh field gives its first mesh, a wrong kind is refused")
{
    const auto& resources = session.project()->resources();
    ResourceCache& cache = session.resources();
    CHECK(resourceForField(MeshData::kResourceKind, model, &resources, cache) == mesh("mesh:Door"));
    CHECK(resourceForField(MeshData::kResourceKind, mesh("mesh:Frame"), &resources, cache) == mesh("mesh:Frame"));
    CHECK(resourceForField(MeshData::kResourceKind, builtin::kCubeMesh, &resources, cache) == builtin::kCubeMesh);
    const ResourceId scene = session.project()->settings().get<GeneralSettings>().startupScene.id;
    CHECK(resourceForField(SceneSerializer::kResourceKind, scene, &resources, cache) == scene);
    CHECK_FALSE(resourceForField(MeshData::kResourceKind, scene, &resources, cache));
    CHECK_FALSE(resourceForField(TextureData::kResourceKind, model, &resources, cache));
    CHECK_FALSE(resourceForField(TextureData::kResourceKind, mesh("mesh:Door"), &resources, cache));
    CHECK_FALSE(resourceForField(TextureData::kResourceKind, builtin::kCubeMesh, &resources, cache));
}

TEST_CASE_FIXTURE(Fixture, "resourceChoices: nested resources follow the file order, a removed one is not offered")
{
    const ResourceId frame = mesh("mesh:Frame");
    GltfBuilder gltf = house();
    gltf.json["meshes"] = {gltf.json["meshes"][1]};
    gltf.json["nodes"][1].erase("mesh");
    gltf.json["nodes"][2].erase("mesh");
    gltf.json["nodes"][3]["mesh"] = 0;
    const fs::path file = directory / "models" / "house.glb";
    const auto time = fs::last_write_time(file);
    gltf.writeGlb(file);
    test::markRewritten(file, time);
    session.refreshResources();

    // Door остался в .meta, но в файле его больше нет.
    const auto& resources = session.project()->resources();
    CHECK(resources.subresources(model).contains("mesh:Door"));
    const auto meshes = resourceChoices(MeshData::kResourceKind, &resources, session.resources());
    REQUIRE(meshes.size() == builtinMeshes().size() + 1);
    CHECK(meshes.back().id == frame);
    CHECK(resourceForField(MeshData::kResourceKind, model, &resources, session.resources()) == frame);
}
