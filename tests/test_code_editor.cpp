#include <doctest/doctest.h>

#include <yqlora/editor/CodeEditor.hpp>

#include <filesystem>
#include <string>
#include <vector>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

const std::filesystem::path kProject = "/home/me/my game";

CodeLocation location(std::filesystem::path file, int line, int column)
{
    return {.file = std::move(file), .line = line, .column = column};
}

} // namespace

TEST_CASE("expandCodeEditorArguments: a path with spaces and quotes stays one argument")
{
    const auto file = std::filesystem::path("/home/me/my game/scripts/\"odd\" name.cpp");
    const auto result = expandCodeEditorArguments("--goto {file}:{line}:{column}", location(file, 12, 5), kProject);
    REQUIRE(result.ok());
    CHECK(result.arguments == std::vector<std::string>{"--goto", "/home/me/my game/scripts/\"odd\" name.cpp:12:5"});

    const auto separate = expandCodeEditorArguments("--line {line} {file}", location(file, 7, 0), kProject);
    REQUIRE(separate.ok());
    CHECK(separate.arguments == std::vector<std::string>{"--line", "7", "/home/me/my game/scripts/\"odd\" name.cpp"});
}

TEST_CASE("expandCodeEditorArguments: an unnamed line and column are 1, the project is its folder")
{
    const auto result = expandCodeEditorArguments(
        "{project} {file}:{line}:{column}",
        location("a.cpp", 0, 0),
        kProject
    );
    REQUIRE(result.ok());
    CHECK(result.arguments == std::vector<std::string>{"/home/me/my game", "a.cpp:1:1"});
}

TEST_CASE("expandCodeEditorArguments: an empty placeholder word is dropped, a quoted empty one stays")
{
    const auto result = expandCodeEditorArguments("{project} \"\" {file}", location("a.cpp", 1, 1), {});
    REQUIRE(result.ok());
    CHECK(result.arguments == std::vector<std::string>{"", "a.cpp"});
}

TEST_CASE("expandCodeEditorArguments: quotes group literal spaces, doubled braces are braces")
{
    const auto result = expandCodeEditorArguments(
        "--title=\"my editor\" {{x}} \"{file} here\"",
        location("a b.cpp", 1, 1),
        {}
    );
    REQUIRE(result.ok());
    CHECK(result.arguments == std::vector<std::string>{"--title=my editor", "{x}", "a b.cpp here"});
}

TEST_CASE("expandCodeEditorArguments: a broken template is an error of the setting, not a silent empty string")
{
    const auto unknown = expandCodeEditorArguments("{file}:{row}", location("a.cpp", 1, 1), {});
    CHECK(unknown.error == "editor.codeEditor.unknownPlaceholder");
    CHECK(unknown.detail == "{row}");
    CHECK(unknown.arguments.empty());

    CHECK(expandCodeEditorArguments("{file", {}, {}).error == "editor.codeEditor.unclosedBrace");
    CHECK(expandCodeEditorArguments("{file} }", {}, {}).error == "editor.codeEditor.strayBrace");
    CHECK(expandCodeEditorArguments("\"{file}", {}, {}).error == "editor.codeEditor.unclosedQuote");
    CHECK(expandCodeEditorArguments("--line {line}", {}, {}).error == "editor.codeEditor.noFile");
    CHECK(expandCodeEditorArguments("", {}, {}).error == "editor.codeEditor.noFile");
}

TEST_CASE("codeEditorPresets: every ready row names a program and has a valid template")
{
    REQUIRE_FALSE(codeEditorPresets().empty());
    for (const CodeEditorPreset& preset : codeEditorPresets()) {
        CAPTURE(preset.name);
        CHECK_FALSE(preset.program.empty());
        const auto result = expandCodeEditorArguments(preset.arguments, location("a.cpp", 3, 4), kProject);
        CHECK(result.ok());
        CHECK_FALSE(result.arguments.empty());
    }
}
