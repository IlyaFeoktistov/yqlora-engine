#pragma once

#include "yqlora/editor/ScriptToolchain.hpp"

#include <filesystem>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace yqlora::editor {

class LogBuffer;

// Что собирать. Поток сборки получает копию и в проект не заглядывает.
struct ScriptBuildSetup {
    std::filesystem::path projectDirectory;
    std::filesystem::path sdkDirectory;
    ScriptToolchain toolchain;
    // CMAKE_BUILD_TYPE, см. EnumNames<ScriptConfiguration>.
    std::string configuration;
    // Сборка не по просьбе человека (окно получило фокус): если собирать
    // нечего, в лог не пишется ничего.
    bool quiet = false;
};

struct ScriptBuildResult {
    bool succeeded = false;
    // Собранная библиотека: путь называет CMake (yqlora_add_game_module).
    std::filesystem::path library;
    // Библиотека другая, чем до сборки: её стоит перезагрузить.
    bool changed = false;
};

// Папка сборки: <проект>/.build/<конфигурация>-<отпечаток набора и SDK>.
// Новый набор инструментов или перенесённый редактор - новая папка.
std::filesystem::path scriptBuildDirectory(const ScriptBuildSetup& setup);

// В проекте есть хоть один .cpp вне скрытых папок: модулю игры есть из чего собираться.
bool hasGameSources(const std::filesystem::path& projectDirectory);

// Библиотека, собранная в папке сборки раньше (до запуска редактора);
// nullopt - её там ещё нет.
std::optional<std::filesystem::path> builtGameLibrary(const ScriptBuildSetup& setup);

// Сборка модуля игры в своём потоке: CMake настраивает папку сборки, только
// когда её ещё нет, а что пересобирать, решает Ninja. Вывод сборки идёт в лог
// с уровнями из CompilerDiagnostics.
//
// Сборка одна за раз. Запрос во время сборки ставит один флаг: сколько бы
// запросов ни пришло, после текущей сборки будет ещё ровно одна, с последней
// постановкой, - исходник, изменённый во время сборки, не потеряется.
class ScriptBuild {
public:
    explicit ScriptBuild(LogBuffer& log);
    // Останавливает идущую сборку.
    ~ScriptBuild();

    ScriptBuild(const ScriptBuild&) = delete;
    ScriptBuild& operator=(const ScriptBuild&) = delete;

    // Главный поток.
    void request(ScriptBuildSetup setup);

    bool building() const;

    // Главный поток: итог последней законченной сборки, один раз.
    std::optional<ScriptBuildResult> takeResult();

    // Останавливает сборку и забывает отложенный запрос и итог (проект закрыт).
    void cancel();

private:
    void run(ScriptBuildSetup setup, std::stop_token stop);
    ScriptBuildResult buildOnce(const ScriptBuildSetup& setup, std::stop_token stop);

    LogBuffer& m_log;
    mutable std::mutex m_mutex;
    bool m_running = false;
    std::optional<ScriptBuildSetup> m_pending;
    std::optional<ScriptBuildResult> m_result;
    // Последним: уходит первым, пока остальное ещё живо.
    std::jthread m_worker;
};

} // namespace yqlora::editor
