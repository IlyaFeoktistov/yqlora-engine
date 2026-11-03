#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceDatabase.hpp>
#include <yqlora/resource/ResourceLoader.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace yqlora;

namespace {

// Как файл проекта: в корне, но не ресурс.
constexpr std::array<std::string_view, 1> kNotResources{"project.yqlora"};

} // namespace

TEST_CASE("ResourceDatabase: files get ids that survive a rescan")
{
    const auto root = test::freshDirectory("resources_scan");
    test::writeFile(root / "player.png", "data");
    test::writeFile(root / "levels" / "one.yscene", "data");
    test::writeFile(root / ".hidden" / "secret.txt", "data");
    test::writeFile(root / "project.yqlora", "data");

    ResourceDatabase database;
    database.scan(root, kNotResources);
    CHECK(database.size() == 2);
    const ResourceId player = database.id("player.png");
    const ResourceId level = database.id("levels/one.yscene");
    CHECK(player != 0);
    CHECK_FALSE(builtin::isBuiltin(player));
    CHECK(level != player);
    CHECK(std::filesystem::exists(root / "player.png.meta"));
    CHECK(database.id("project.yqlora") == 0);

    ResourceDatabase again;
    again.scan(root, kNotResources);
    CHECK(again.id("player.png") == player);
    CHECK(again.path(level) == std::filesystem::path("levels/one.yscene"));
}

TEST_CASE("ResourceDatabase: a copied file with its .meta gets a new id")
{
    const auto root = test::freshDirectory("resources_copy");
    test::writeFile(root / "a.png", "data");
    ResourceDatabase database;
    database.scan(root);
    std::filesystem::copy_file(root / "a.png", root / "b.png");
    std::filesystem::copy_file(root / "a.png.meta", root / "b.png.meta");

    database.scan(root);
    CHECK(database.size() == 2);
    CHECK(database.id("a.png") != database.id("b.png"));
}

TEST_CASE("ResourceDatabase: ids follow renamed files and folders, removed ones are forgotten")
{
    const auto root = test::freshDirectory("resources_move");
    test::writeFile(root / "art" / "hero.png", "data");
    test::writeFile(root / "notes.txt", "data");
    ResourceDatabase database;
    database.scan(root);
    const ResourceId hero = database.id("art/hero.png");
    const ResourceId notes = database.id("notes.txt");

    std::filesystem::rename(root / "notes.txt", root / "readme.txt");
    database.moved("notes.txt", "readme.txt");
    CHECK(database.id("readme.txt") == notes);
    CHECK(std::filesystem::exists(root / "readme.txt.meta"));
    CHECK_FALSE(std::filesystem::exists(root / "notes.txt.meta"));

    std::filesystem::rename(root / "art", root / "sprites");
    database.moved("art", "sprites");
    CHECK(database.path(hero) == std::filesystem::path("sprites/hero.png"));

    std::filesystem::remove_all(root / "sprites");
    database.removed("sprites");
    CHECK_FALSE(database.path(hero));

    // Новый файл получает ID сразу, без повторного обхода.
    test::writeFile(root / "new.png", "data");
    const ResourceId created = database.add("new.png");
    CHECK(created != 0);
    CHECK(database.add("new.png") == created);
}

namespace {

std::string readText(const std::filesystem::path& file)
{
    std::ifstream stream(file);

    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("ResourceDatabase: nested ids are made once, kept in .meta and survive a rescan and a rename")
{
    const auto root = test::freshDirectory("resources_nested");
    test::writeFile(root / "door.glb", "data");
    ResourceDatabase database;
    database.scan(root);
    const ResourceId file = database.id("door.glb");

    const std::vector<std::string> keys{"mesh:Door", "mesh:Door#1"};
    const auto ids = database.assignSubresources(file, keys);
    REQUIRE(ids.size() == 2);
    CHECK(ids[0] != ids[1]);
    CHECK(database.fileOf(ids[0]) == file);
    CHECK(database.fileOf(file) == file);
    CHECK(database.key(ids[1]) == "mesh:Door#1");
    CHECK(database.key(file).empty());
    CHECK(database.path(ids[0]) == std::filesystem::path("door.glb"));
    // Вложенные ресурсы - не файлы.
    CHECK(database.paths().size() == 1);
    CHECK(readText(root / "door.glb.meta").find("\"mesh:Door#1\"") != std::string::npos);

    // Перевыгрузка: известные ключи - те же ID, новый ключ - новый ID, а
    // запись пропавшего остаётся.
    const auto again = database.assignSubresources(file, std::vector<std::string>{"mesh:Door#1", "mesh:Frame"});
    CHECK(again[0] == ids[1]);
    CHECK(again[1] != ids[0]);
    CHECK(database.subresources(file).size() == 3);
    CHECK(database.fileOf(ids[0]) == file);

    std::filesystem::rename(root / "door.glb", root / "gate.glb");
    database.moved("door.glb", "gate.glb");
    ResourceDatabase reopened;
    reopened.scan(root);
    CHECK(reopened.id("gate.glb") == file);
    CHECK(reopened.subresources(file).at("mesh:Door") == ids[0]);
    CHECK(reopened.path(ids[0]) == std::filesystem::path("gate.glb"));

    reopened.removed("gate.glb");
    CHECK(reopened.fileOf(ids[0]) == 0);
    CHECK(reopened.key(ids[0]).empty());
}

TEST_CASE("ResourceDatabase: a copy of a model gets a new id for the file and every nested resource")
{
    const auto root = test::freshDirectory("resources_nested_copy");
    test::writeFile(root / "a.glb", "data");
    ResourceDatabase database;
    database.scan(root);
    const ResourceId original = database.id("a.glb");
    const ResourceId mesh = database.assignSubresources(original, std::vector<std::string>{"mesh:Box"})[0];
    std::filesystem::copy_file(root / "a.glb", root / "b.glb");
    std::filesystem::copy_file(root / "a.glb.meta", root / "b.glb.meta");

    database.scan(root);
    const ResourceId copy = database.id("b.glb");
    CHECK(database.id("a.glb") == original);
    CHECK(copy != original);
    const ResourceId copiedMesh = database.subresources(copy).at("mesh:Box");
    CHECK(copiedMesh != mesh);
    CHECK(database.fileOf(mesh) == original);
    CHECK(database.fileOf(copiedMesh) == copy);
}

TEST_CASE("ResourceDatabase: import settings are kept in .meta, a .meta without a format version is broken")
{
    const auto root = test::freshDirectory("resources_import");
    test::writeFile(root / "grass.png", "data");
    ResourceDatabase database;
    database.scan(root);
    const ResourceId file = database.id("grass.png");
    CHECK(database.importSettings(file) == "{}");
    CHECK_FALSE(database.setImportSettings(file, "[1, 2]"));
    CHECK_FALSE(database.setImportSettings(file, "not json"));
    REQUIRE(database.setImportSettings(file, R"({"srgb": false})"));

    ResourceDatabase reopened;
    reopened.scan(root);
    CHECK(reopened.id("grass.png") == file);
    CHECK(reopened.importSettings(file) == R"({"srgb":false})");

    // Формат .meta - версии 1 без совместимости: .meta без версии не читается.
    std::ofstream(root / "grass.png.meta", std::ios::trunc) << R"({"id": "00000000000f4240"})";
    reopened.scan(root);
    CHECK(reopened.id("grass.png") != 0x00000000000f4240);
}

TEST_CASE("SubresourceKeys: names, repeated names, names with # and nameless elements give distinct keys")
{
    SubresourceKeys keys;
    CHECK(keys.next("mesh", "Door", 0) == "mesh:Door");
    CHECK(keys.next("mesh", "Door", 1) == "mesh:Door#1");
    CHECK(keys.next("mesh", "Door#1", 2) == "mesh:Door#1#1");
    CHECK(keys.next("mesh", "", 3) == "mesh:#3");
    CHECK(keys.next("texture", "Door", 0) == "texture:Door");
}
