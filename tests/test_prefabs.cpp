#include "EngineRegistry.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/JsonFile.hpp>
#include <yqlora/core/JsonId.hpp>
#include <yqlora/import/SceneFileLoader.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceKinds.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Prefab.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneBranches.hpp>
#include <yqlora/scene/SceneSerializer.hpp>

#include <filesystem>
#include <memory>
#include <string>

using namespace yqlora;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

struct Fixture {
    Fixture()
    {
        auto& root = scene.createRoot<Node3D>("Door");
        auto& leaf = root.addChild<Node3D>("Leaf");
        leaf.addComponent<MeshRenderer>().setMesh(ResourceRef{builtin::kCubeMesh});
    }

    // Пишет сцену файлом path и читает его загрузчиком вида kind.
    LoadResult saveAndLoad(const fs::path& path, std::string_view kind) const
    {
        REQUIRE(serializer.save(scene, path));
        const SceneFileLoader loader(path.extension().string(), kind);

        return loader.load(LoadRequest{.file = path});
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    SceneSerializer serializer{registry};
    Scene scene{services};
};

const Json& rootOf(const LoadResult& result)
{
    REQUIRE(result.error.empty());
    REQUIRE(result.resources.size() == 1);

    return static_cast<const SceneTree*>(result.resources.front().data.get())->root;
}

} // namespace

TEST_CASE_FIXTURE(Fixture, "Prefab and scene files load as trees with their IDs, under their own kinds")
{
    const fs::path folder = test::freshDirectory("prefab_files");
    const LoadResult prefab = saveAndLoad(folder / "Door.yprefab", kinds::kPrefab);
    CHECK(prefab.resources.front().kind == kinds::kPrefab);
    const Json& root = rootOf(prefab);
    CHECK(idFromJson(root["id"]) == scene.root()->id());
    CHECK(root["children"][0]["properties"]["name"] == "Leaf");

    const LoadResult level = saveAndLoad(folder / "Level.yscene", kinds::kScene);
    CHECK(level.resources.front().kind == kinds::kScene);
    CHECK(idFromJson(rootOf(level)["id"]) == scene.root()->id());

    // Файл новее движка не читается и не выдаётся за пустой.
    Json future = *readJsonFile(folder / "Door.yprefab");
    future["version"] = 99;
    REQUIRE(writeJsonFile(folder / "Future.yprefab", future));
    const LoadResult refused = SceneFileLoader(kPrefabFileExtension, kinds::kPrefab)
                                   .load(LoadRequest{.file = folder / "Future.yprefab"});
    CHECK_FALSE(refused.error.empty());
}

TEST_CASE("A prefab field takes prefabs and models, never scenes")
{
    CHECK(kinds::fitsKind(kinds::kPrefab, kinds::kPrefab));
    CHECK(kinds::fitsKind(kinds::kModel, kinds::kPrefab));
    CHECK_FALSE(kinds::fitsKind(kinds::kScene, kinds::kPrefab));
    // Обратное правило не действует: поле модели префаб не берёт.
    CHECK_FALSE(kinds::fitsKind(kinds::kPrefab, kinds::kModel));
}

TEST_CASE_FIXTURE(Fixture, "SceneBranches: a branch keeps its IDs, or gets new ones for a copy")
{
    const Node& source = *scene.root();
    const Json branch = SceneBranches::write(registry, source);

    const auto same = SceneBranches::read(registry, branch, BranchIds::FromFile);
    REQUIRE(same);
    CHECK(same->id() == source.id());
    REQUIRE(same->children().size() == 1);
    const Node& leaf = *same->children().front();
    CHECK(leaf.id() == source.children().front()->id());
    CHECK(leaf.getComponent<MeshRenderer>()->id() == source.children().front()->getComponent<MeshRenderer>()->id());

    const auto copy = SceneBranches::read(registry, branch, BranchIds::Fresh);
    REQUIRE(copy);
    CHECK(copy->id() != source.id());
    CHECK(copy->name() == "Door");
    const Node& copiedLeaf = *copy->children().front();
    CHECK(copiedLeaf.id() != source.children().front()->id());
    const MeshRenderer* renderer = copiedLeaf.getComponent<MeshRenderer>();
    REQUIRE(renderer);
    CHECK(renderer->mesh().id == builtin::kCubeMesh);
    CHECK(renderer->id() != source.children().front()->getComponent<MeshRenderer>()->id());
}
