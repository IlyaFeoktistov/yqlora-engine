#include "yqlora/editor/ScriptBuild.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/CompilerDiagnostics.hpp"
#include "yqlora/editor/LogBuffer.hpp"
#include "yqlora/platform/Files.hpp"
#include "yqlora/platform/Platform.hpp"
#include "yqlora/platform/Process.hpp"
#include "yqlora/project/Project.hpp"

#include <chrono>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace yqlora::editor {

namespace {

namespace fs = std::filesystem;

// Запись пути к библиотеке, которую пишет yqlora_add_game_module (cmake/YqloraGameModule.cmake).
constexpr const char* kModuleRecordFile = "yqlora_game_module.txt";
// Так Ninja говорит, что всё собрано, и так проверяет список файлов сборки
// (file(GLOB CONFIGURE_DEPENDS)) перед каждой сборкой: в логе это не новость.
constexpr std::string_view kNothingToDo = "ninja: no work to do.";
constexpr std::string_view kGlobCheck = "Re-checking globbed directories...";
// Меняется, когда меняется то, как настраивается папка сборки (например,
// короткие пути): старая папка тогда не годится, и сборка берёт новую.
constexpr std::string_view kBuildLayout = "game-project\n";
// Пакет и CMakeLists.txt модуля игры внутри SDK, см. cmake/Sdk.cmake.
constexpr const char* kSdkCmakeFolder = "cmake";
constexpr const char* kGameProjectFolder = "game-project";
// Единицы трансляции модуля игры; так же их ищет cmake/game-project/CMakeLists.txt.
constexpr std::string_view kCompiledExtension = ".cpp";

// FNV-1a: отпечаток одинаков на всех машинах и сборках редактора, в отличие от std::hash.
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::uint64_t fnv1a(std::string_view text)
{
    std::uint64_t hash = kFnvOffset;
    for (const char c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= kFnvPrime;
    }

    return hash;
}

std::string trimmed(std::string text)
{
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
        text.pop_back();
    }

    return text;
}

std::optional<fs::path> readModuleRecord(const fs::path& buildDirectory)
{
    const auto text = platform::readFile(buildDirectory / kModuleRecordFile);
    if (!text) {
        return std::nullopt;
    }

    return fromUtf8(trimmed(*text));
}

// Сообщения одной сборки в лог редактора. Тихая сборка, которой нечего
// делать, не оставляет в логе ни строки: начало пишется перед первым
// сообщением, а у обычной - сразу.
class BuildMessages {
public:
    BuildMessages(LogBuffer& log, const ScriptBuildSetup& setup)
        : m_log(log)
        , m_configuration(setup.configuration)
    {
        if (!setup.quiet) {
            announce();
        }
    }

    bool announced() const
    {
        return m_announced;
    }

    // Строка лога как есть.
    void add(log::Level level, std::string message)
    {
        m_log.add({.level = level, .source = LogSource::Build, .message = std::move(message)});
    }

    // Строка о сборке: перед первой - начало сборки.
    void note(log::Level level, std::string message)
    {
        announce();
        add(level, std::move(message));
    }

private:
    void announce()
    {
        if (!m_announced) {
            m_announced = true;
            add(log::Level::Info, std::format("Building scripts ({})", m_configuration));
        }
    }

    LogBuffer& m_log;
    std::string m_configuration;
    bool m_announced = false;
};

// Папка сборки и пути, которые получают CMake и Ninja.
struct BuildPaths {
    fs::path directory;
    // Ninja на Windows не находит файлы по путям не из ASCII и пересобирал бы
    // всё каждый раз: CMake и Ninja получают короткие пути (8.3).
    fs::path project{};
    fs::path build{};
    fs::path sdk{};
};

// Создаёт папку сборки; без коротких путей - предупреждение в лог.
BuildPaths buildPaths(const ScriptBuildSetup& setup, BuildMessages& messages)
{
    BuildPaths paths{.directory = scriptBuildDirectory(setup)};
    std::error_code ec;
    fs::create_directories(paths.directory, ec);
    const auto project = platform::asciiPath(setup.projectDirectory);
    const auto build = platform::asciiPath(paths.directory);
    const auto sdk = platform::asciiPath(setup.sdkDirectory);
    paths.project = project.value_or(setup.projectDirectory);
    paths.build = build.value_or(paths.directory);
    paths.sdk = sdk.value_or(setup.sdkDirectory);
    if (!project || !build || !sdk) {
        messages.note(
            log::Level::Warning,
            "The project or editor path has non-ASCII characters and no short name: Ninja may rebuild "
            "everything every time. Move the project to a path in Latin letters"
        );
    }

    return paths;
}

// Аргументы CMake, которые настраивают папку сборки модуля игры.
std::vector<std::string> configureArguments(const ScriptBuildSetup& setup, const BuildPaths& paths)
{
    std::vector<std::string> arguments{
        "-S",
        toUtf8(paths.sdk / kGameProjectFolder),
        "-B",
        toUtf8(paths.build),
        "-G",
        setup.toolchain.generator,
        // C-компилятор передаётся всегда, а проект может быть только на C++.
        "--no-warn-unused-cli",
        "-DCMAKE_BUILD_TYPE=" + setup.configuration,
        "-Dyqlora_DIR=" + toUtf8(paths.sdk / kSdkCmakeFolder),
        "-DYQLORA_PROJECT_DIR=" + toUtf8(paths.project),
    };
    if (!setup.toolchain.makeProgram.empty()) {
        arguments.push_back("-DCMAKE_MAKE_PROGRAM=" + toUtf8(setup.toolchain.makeProgram));
    }
    if (!setup.toolchain.cCompiler.empty()) {
        arguments.push_back("-DCMAKE_C_COMPILER=" + toUtf8(setup.toolchain.cCompiler));
    }
    arguments.push_back("-DCMAKE_CXX_COMPILER=" + toUtf8(setup.toolchain.cxxCompiler));

    return arguments;
}

// Запускает CMake из набора инструментов; его строки идут в лог, кроме
// тех, что не новость. true, если он отработал без ошибки.
bool runCMake(
    const ScriptBuildSetup& setup,
    std::vector<std::string> arguments,
    BuildMessages& messages,
    std::stop_token stop
)
{
    const platform::ProcessOptions options{
        .program = setup.toolchain.cmake,
        .arguments = std::move(arguments),
        .workingDirectory = {},
    };
    const platform::ProcessResult result = platform::runProcess(
        options,
        [&](std::string_view line) {
            if (line != kNothingToDo && !line.ends_with(kGlobCheck)) {
                messages.note(parseCompilerOutput(line).level, std::string(line));
            }
        },
        stop
    );
    if (!result.started) {
        messages.note(log::Level::Error, std::format("Cannot run '{}': {}", toUtf8(options.program), result.error));
    }

    return result.succeeded();
}

} // namespace

fs::path scriptBuildDirectory(const ScriptBuildSetup& setup)
{
    const std::string key = std::string(kBuildLayout) + setup.toolchain.fingerprint() + toUtf8(setup.sdkDirectory);

    return setup.projectDirectory / Project::buildDirectoryName()
           / std::format("{}-{:016x}", setup.configuration, fnv1a(key));
}

bool hasGameSources(const fs::path& projectDirectory)
{
    std::error_code ec;
    auto it = fs::recursive_directory_iterator(projectDirectory, ec);
    for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        const std::string name = toUtf8(it->path().filename());
        if (!name.empty() && name.front() == '.') {
            if (it->is_directory(ec)) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (it->path().extension() == kCompiledExtension && it->is_regular_file(ec)) {
            return true;
        }
    }

    return false;
}

std::optional<fs::path> builtGameLibrary(const ScriptBuildSetup& setup)
{
    auto library = readModuleRecord(scriptBuildDirectory(setup));
    std::error_code ec;
    if (!library || !fs::exists(*library, ec)) {
        return std::nullopt;
    }

    return library;
}

ScriptBuild::ScriptBuild(LogBuffer& log)
    : m_log(log)
{
}

ScriptBuild::~ScriptBuild()
{
    cancel();
}

void ScriptBuild::request(ScriptBuildSetup setup)
{
    const std::scoped_lock lock(m_mutex);
    if (m_running) {
        // Хоть один громкий запрос - и отложенная сборка громкая.
        setup.quiet = setup.quiet && (!m_pending || m_pending->quiet);
        m_pending = std::move(setup);

        return;
    }
    // Прошлый поток уже закончил (m_running сброшен), join не ждёт.
    if (m_worker.joinable()) {
        m_worker.join();
    }
    m_running = true;
    m_worker = std::jthread([this, setup = std::move(setup)](std::stop_token stop) mutable {
        run(std::move(setup), stop);
    });
}

bool ScriptBuild::building() const
{
    const std::scoped_lock lock(m_mutex);

    return m_running;
}

std::optional<ScriptBuildResult> ScriptBuild::takeResult()
{
    const std::scoped_lock lock(m_mutex);

    return std::exchange(m_result, std::nullopt);
}

void ScriptBuild::cancel()
{
    {
        const std::scoped_lock lock(m_mutex);
        m_pending.reset();
    }
    if (m_worker.joinable()) {
        m_worker.request_stop();
        m_worker.join();
    }
    const std::scoped_lock lock(m_mutex);
    m_running = false;
    m_result.reset();
}

void ScriptBuild::run(ScriptBuildSetup setup, std::stop_token stop)
{
    while (true) {
        ScriptBuildResult result = buildOnce(setup, stop);
        const std::scoped_lock lock(m_mutex);
        m_result = std::move(result);
        if (stop.stop_requested() || !m_pending) {
            m_running = false;

            return;
        }
        setup = std::move(*m_pending);
        m_pending.reset();
    }
}

ScriptBuildResult ScriptBuild::buildOnce(const ScriptBuildSetup& setup, std::stop_token stop)
{
    const auto started = std::chrono::steady_clock::now();
    BuildMessages messages(m_log, setup);
    ScriptBuildResult result;
    // vcvarsall.bat ставит окружение только в своём cmd: запуск CMake через
    // него ещё не сделан, поэтому движок, собранный MSVC, проекты пока не собирает.
    if (!setup.toolchain.vcvarsall.empty()) {
        messages.note(log::Level::Error, "Building scripts with MSVC from the editor is not supported yet");

        return result;
    }

    const BuildPaths paths = buildPaths(setup, messages);
    std::error_code ec;
    // Запись появляется, только когда CMake настроил папку до конца: после
    // неудачной настройки следующая сборка настроит её снова. Дальше CMake
    // перенастраивает сам, когда меняются CMakeLists.txt.
    if (!fs::exists(paths.directory / kModuleRecordFile, ec)
        && !runCMake(setup, configureArguments(setup, paths), messages, stop)) {
        messages.note(log::Level::Error, stop.stop_requested() ? "Build stopped" : "Configuring scripts failed");

        return result;
    }
    const auto previous = readModuleRecord(paths.directory);
    const auto previousTime = previous ? fs::last_write_time(*previous, ec) : fs::file_time_type{};
    if (!runCMake(setup, {"--build", toUtf8(paths.build)}, messages, stop)) {
        messages.note(log::Level::Error, stop.stop_requested() ? "Build stopped" : "Building scripts failed");

        return result;
    }

    const auto library = readModuleRecord(paths.directory);
    if (!library || !fs::exists(*library, ec)) {
        messages.note(log::Level::Error, "The build did not produce the game module");

        return result;
    }
    result.succeeded = true;
    result.library = *library;
    result.changed = library != previous || fs::last_write_time(*library, ec) != previousTime;
    if (messages.announced()) {
        const std::chrono::duration<float> elapsed = std::chrono::steady_clock::now() - started;
        messages.add(log::Level::Info, std::format("Scripts built in {:.1f} s", elapsed.count()));
    }

    return result;
}

} // namespace yqlora::editor
