#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/core/Utf8Path.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/LogBuffer.hpp>
#include <yqlora/editor/ProjectFiles.hpp>
#include <yqlora/editor/ProjectSetup.hpp>
#include <yqlora/editor/ScriptBuild.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/scene/Node.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scripting/GameModule.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace yqlora;
using namespace yqlora::editor;

namespace fs = std::filesystem;

namespace {

// Настоящая сборка CMake: настройка и компиляция шаблона занимают секунды.
constexpr auto kBuildTimeout = std::chrono::minutes(5);
constexpr auto kPollInterval = std::chrono::milliseconds(20);

// Скрипт с полями для проверок: заголовок отдельно, чтобы правка только
// заголовка пересобирала модуль. Лежит во вложенной папке: сборка находит
// .cpp где угодно в проекте.
constexpr std::string_view kPulseHeader = R"(#pragma once

#include <yqlora/scripting/Script.hpp>

class Pulse : public yqlora::Script {
public:
    YQLORA_SCRIPT(Pulse)

    float amplitude = 0.2f;
    float frequency = 1.0f;

    YQLORA_FIELDS(field("amplitude", &Pulse::amplitude), field("frequency", &Pulse::frequency))
};
)";
constexpr std::string_view kPulseSource = R"(#include "Pulse.hpp"

#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scripting/ModuleScripts.hpp>

YQLORA_REGISTER_SCRIPT(Pulse).requiresNode<yqlora::Node3D>();
)";

// Проект с двумя скриптами: Pulse выше и Spinner из шаблона редактора.
// Сборка - тем же набором инструментов, что у движка.
struct BuildFixture {
    TypeRegistry registry;
    fs::path project;
    ScriptBuildSetup setup;
    LogBuffer log;
    ScriptBuild build{log};

    explicit BuildFixture(std::string_view name)
    {
        registerEngineTypes(registry);
        const SceneSerializer serializer(registry);
        const auto location = test::freshDirectory(fromUtf8(name));
        std::string error;
        auto created = createProject(location, "Game", serializer, error);
        REQUIRE(created);
        project = created->directory();
        const auto folder = project / "scripts" / "effects";
        fs::create_directories(folder);
        std::ofstream(folder / "Pulse.hpp") << kPulseHeader;
        std::ofstream(folder / "Pulse.cpp") << kPulseSource;
        ProjectFiles files(*created);
        REQUIRE(files.createScript("scripts", "Spinner", YQLORA_SCRIPT_TEMPLATE_DIR));
        const auto toolchain = loadScriptToolchain(YQLORA_SDK_DIR);
        REQUIRE(toolchain);
        setup = {
            .projectDirectory = project,
            .sdkDirectory = YQLORA_SDK_DIR,
            .toolchain = *toolchain,
            .configuration = "Debug"
        };
    }

    ScriptBuildResult wait()
    {
        const auto deadline = std::chrono::steady_clock::now() + kBuildTimeout;
        while (build.building() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(kPollInterval);
        }
        REQUIRE_FALSE(build.building());
        auto result = build.takeResult();
        REQUIRE(result);
        log.collect();

        return *result;
    }

    ScriptBuildResult buildNow()
    {
        build.request(setup);

        return wait();
    }

    std::size_t count(std::string_view message) const
    {
        return static_cast<std::size_t>(std::ranges::count_if(log.entries(), [&](const LogEntry& entry) {
            return entry.message.starts_with(message);
        }));
    }

    // Файл новее всего, что из него собрано, даже если файловая система
    // хранит время грубо.
    void touchAfter(const fs::path& file, const fs::path& output)
    {
        test::markRewritten(file, fs::last_write_time(output));
    }
};

} // namespace

TEST_CASE("ScriptBuild: builds the project scripts; a header change rebuilds the module, no change does not")
{
    BuildFixture fixture("script_build");
    const auto first = fixture.buildNow();
    REQUIRE(first.succeeded);
    CHECK(fs::exists(first.library));
    CHECK(first.library.parent_path() == scriptBuildDirectory(fixture.setup));
    const auto built = fs::last_write_time(first.library);

    CHECK(first.changed);

    // Тихая сборка без изменений не пишет в лог ничего.
    fixture.log.clear();
    fixture.setup.quiet = true;
    const auto unchanged = fixture.buildNow();
    REQUIRE(unchanged.succeeded);
    CHECK_FALSE(unchanged.changed);
    CHECK(fs::last_write_time(unchanged.library) == built);
    CHECK(fixture.log.entries().empty());

    const auto header = fixture.project / "scripts" / "effects" / "Pulse.hpp";
    std::ofstream(header, std::ios::app) << "\n// changed\n";
    fixture.touchAfter(header, first.library);
    const auto rebuilt = fixture.buildNow();
    REQUIRE(rebuilt.succeeded);
    CHECK(rebuilt.changed);
    CHECK(fixture.count("Building scripts (") == 1);
    CHECK(fs::last_write_time(rebuilt.library) > built);
}

TEST_CASE("ScriptBuild: compiler errors go to the log as errors, the build fails")
{
    BuildFixture fixture("script_build_error");
    std::ofstream(fixture.project / "scripts" / "effects" / "Pulse.cpp", std::ios::app) << "\nthis is not C++\n";
    const auto result = fixture.buildNow();
    CHECK_FALSE(result.succeeded);
    CHECK(fixture.log.count(log::Level::Error) > 0);
    CHECK(fixture.count("Building scripts failed") == 1);
}

TEST_CASE("ScriptBuild: requests during a build run exactly one more build")
{
    BuildFixture fixture("script_build_pending");
    fixture.build.request(fixture.setup);
    fixture.build.request(fixture.setup);
    fixture.build.request(fixture.setup);
    const auto result = fixture.wait();
    CHECK(result.succeeded);
    CHECK(fixture.count("Building scripts (") == 2);
}

TEST_CASE("ScriptBuild: a changed toolchain or configuration gets its own build folder")
{
    BuildFixture fixture("script_build_folders");
    auto other = fixture.setup;
    other.configuration = "Release";
    CHECK(scriptBuildDirectory(other) != scriptBuildDirectory(fixture.setup));
    other = fixture.setup;
    other.toolchain.compilerVersion += ".1";
    CHECK(scriptBuildDirectory(other) != scriptBuildDirectory(fixture.setup));
    CHECK(scriptBuildDirectory(fixture.setup).parent_path() == fixture.project / ".build");
}

TEST_CASE("ScriptBuild: build, open, change the script, rebuild and reload; field values survive")
{
    BuildFixture fixture("script_build_reload");
    const auto built = fixture.buildNow();
    REQUIRE(built.succeeded);

    Input input;
    EngineServices services{input, fixture.registry};
    GameModule module{fixture.registry};
    const SceneSerializer serializer(fixture.registry);
    EditorSession session{services, serializer, module};
    REQUIRE(session.open(*Project::open(fixture.project, fixture.registry), built.library));
    const TypeInfo* pulse = fixture.registry.find("Pulse");
    REQUIRE(pulse);
    CHECK(fixture.registry.find("Spinner"));
    Node& root = *session.scene().root();
    Component* script = session.editing().addComponent(root, *pulse);
    REQUIRE(script);
    const auto field = [&](std::string_view name) -> const Property* {
        const auto properties = fixture.registry.properties(*fixture.registry.find("Pulse"));
        const auto it = std::ranges::find(properties, name, &Property::name);

        return it != properties.end() ? *it : nullptr;
    };
    REQUIRE(field("amplitude")->set(*script, PropertyValue{0.75f}));
    REQUIRE(session.save());

    // Новое поле в скрипте: после пересборки и перезагрузки оно есть, а
    // значение старого - то, что было в сцене.
    const auto header = fixture.project / "scripts" / "effects" / "Pulse.hpp";
    std::ifstream in(header);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    const auto replace = [&](std::string_view from, std::string_view to) {
        const auto at = text.find(from);
        REQUIRE(at != std::string::npos);
        text.replace(at, from.size(), to);
    };
    replace("    float frequency = 1.0f;", "    float frequency = 1.0f;\n    float phase = 0.5f;");
    replace(
        "field(\"frequency\", &Pulse::frequency)",
        "field(\"phase\", &Pulse::phase), field(\"frequency\", &Pulse::frequency)"
    );
    std::ofstream(header, std::ios::trunc) << text;
    // Новый файл сборка находит сама, без правки файлов сборки.
    ProjectFiles files(*session.project());
    REQUIRE(files.createScript("scripts", "Walker", YQLORA_SCRIPT_TEMPLATE_DIR));
    fixture.touchAfter(header, built.library);

    const auto rebuilt = fixture.buildNow();
    REQUIRE(rebuilt.succeeded);
    REQUIRE(rebuilt.changed);
    REQUIRE(session.reloadGameModule(rebuilt.library));
    const auto live = session.scene().componentsOfType("Pulse");
    REQUIRE(live.size() == 1);
    REQUIRE(field("phase"));
    CHECK(field("amplitude")->get(*live.front()) == PropertyValue{0.75f});
    CHECK(field("phase")->get(*live.front()) == PropertyValue{0.5f});
    CHECK(fixture.registry.find("Walker"));

    session.close();
    CHECK(fixture.registry.find("Pulse") == nullptr);
}

TEST_CASE("ScriptBuild: a project path with non-ASCII letters does not rebuild everything every time")
{
    BuildFixture fixture("сборка_скриптов");
    REQUIRE(fixture.buildNow().succeeded);

    fixture.log.clear();
    fixture.setup.quiet = true;
    const auto again = fixture.buildNow();
    REQUIRE(again.succeeded);
    CHECK_FALSE(again.changed);
    CHECK(fixture.log.entries().empty());
}
