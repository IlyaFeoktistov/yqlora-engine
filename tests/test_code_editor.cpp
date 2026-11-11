#include <doctest/doctest.h>

#include <yqlora/core/Utf8Path.hpp>
#include <yqlora/editor/CodeEditor.hpp>
#include <yqlora/shader/ShaderCompiler.hpp>

#include <filesystem>
#include <fstream>
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

TEST_CASE("codeEditorOpening: a diagnostic opens at its line and column")
{
    const auto directory = std::filesystem::temp_directory_path() / "yqlora_tests" / "code editor" / "build";
    std::filesystem::create_directories(directory);
    const auto source = directory.parent_path() / "Player.cpp";
    std::ofstream(source) << "int main() {}\n";

    // Ninja печатает путь от папки сборки.
    const CompilerDiagnostic diagnostic = parseCompilerOutput("../Player.cpp:12:5: error: expected ';'");
    const auto location = diagnosticLocation(diagnostic, directory);
    REQUIRE(location);
    CHECK(location->file == source.lexically_normal());
    CHECK(location->line == 12);
    CHECK(location->column == 5);

    const CodeEditorSetting setting{.program = "code", .arguments = "--goto {file}:{line}:{column}"};
    const auto opening = codeEditorOpening(setting, *location, kProject);
    REQUIRE(opening.way == CodeEditorOpening::Way::Program);
    CHECK(opening.process.program == "code");
    CHECK(opening.process.workingDirectory == kProject);
    CHECK(opening.process.arguments == std::vector<std::string>{"--goto", toUtf8(source.lexically_normal()) + ":12:5"});
}

TEST_CASE("diagnosticLocation: no place or no file on disk gives nothing to open")
{
    CHECK_FALSE(diagnosticLocation(parseCompilerOutput("[2/3] Linking CXX shared library"), {}));
    CHECK_FALSE(diagnosticLocation(parseCompilerOutput("engine:include/yqlora/uniforms.glsl:3: error: x"), {}));
}

TEST_CASE("codeEditorOpening: an empty program falls back to the system, a broken template is an error")
{
    const CodeLocation place = location("/home/me/a.cpp", 3, 1);
    const auto system = codeEditorOpening({}, place, kProject);
    CHECK(system.way == CodeEditorOpening::Way::System);
    CHECK(system.file == place.file);

    const auto broken = codeEditorOpening({.program = "code", .arguments = "{row}"}, place, kProject);
    CHECK(broken.way == CodeEditorOpening::Way::Error);
    CHECK(broken.error == "editor.codeEditor.unknownPlaceholder");
}

TEST_CASE("isCodeFile: C++ sources and shaders open in the code editor, other files with the system")
{
    CHECK(isCodeFile("scripts/Player.cpp"));
    CHECK(isCodeFile("scripts/Player.hpp"));
    CHECK(isCodeFile("shaders/water.yshader"));
    CHECK(isCodeFile("shaders/noise.glsl"));
    CHECK_FALSE(isCodeFile("models/tree.glb"));
    CHECK_FALSE(isCodeFile("textures/bark.png"));
}

TEST_CASE("diagnosticLocation: a shader error line names its file from the project folder")
{
    const auto project = std::filesystem::temp_directory_path() / "yqlora_tests" / "code editor" / "project";
    std::filesystem::create_directories(project / "shaders");
    std::ofstream(project / "shaders" / "water.yshader") << "void main() {}\n";

    // Строка в том виде, в каком компилятор шейдеров пишет её в лог.
    const ShaderMessage message{.file = "shaders/water.yshader", .line = 7, .text = "'x' : undeclared identifier"};
    const auto location = diagnosticLocation(parseCompilerOutput(formatShaderMessage(message)), project);
    REQUIRE(location);
    CHECK(location->file == (project / "shaders" / "water.yshader").lexically_normal());
    CHECK(location->line == 7);
}
