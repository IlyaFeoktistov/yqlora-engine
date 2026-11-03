#include <doctest/doctest.h>

#include <yqlora/core/Text.hpp>
#include <yqlora/platform/Files.hpp>

#include <filesystem>
#include <string>

using namespace yqlora;
namespace fs = std::filesystem;

TEST_CASE("Text: ASCII lowercase leaves other bytes as they are")
{
    CHECK(toLowerAscii("MeshRenderer.GLB") == "meshrenderer.glb");
    // Кириллица в UTF-8 не трогается: системная локаль её не портит.
    CHECK(toLowerAscii("Дом Door") == "Дом door");
    CHECK(containsIgnoringCase("PointLight3D", "light3"));
    CHECK(containsIgnoringCase("anything", ""));
    CHECK_FALSE(containsIgnoringCase("Camera3D", "light"));
}

TEST_CASE("Files: a file is written atomically and read back whole")
{
    const fs::path directory = fs::temp_directory_path() / "yqlora_tests" / "files";
    fs::remove_all(directory);
    const fs::path file = directory / "nested" / "data.bin";
    const std::string bytes("line\r\n\0binary", 13);

    REQUIRE(platform::writeFileAtomically(file, bytes));
    CHECK(platform::readFile(file) == bytes);
    CHECK_FALSE(fs::exists(fs::path(file) += ".tmp"));
    REQUIRE(platform::writeFileAtomically(file, "new"));
    CHECK(platform::readFile(file) == "new");
    CHECK_FALSE(platform::readFile(directory / "missing.bin"));
    // В папку записать файл нельзя: прежнее остаётся, временного нет.
    CHECK_FALSE(platform::writeFileAtomically(directory / "nested", "x"));
    CHECK(fs::is_directory(directory / "nested"));
    CHECK_FALSE(fs::exists(directory / "nested.tmp"));
}
