#pragma once

#include "yqlora/core/Export.hpp"

#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora::platform {

struct ProcessOptions {
    std::filesystem::path program;
    // UTF-8; кавычки и экранирование для системы ставит runProcess.
    std::vector<std::string> arguments;
    // Пусто - текущая папка процесса.
    std::filesystem::path workingDirectory;
};

struct ProcessResult {
    bool started = false;
    // Остановлен запросом stop, а не завершился сам.
    bool stopped = false;
    // Код завершения программы. Упавшая программа: на POSIX - минус номер
    // сигнала, на Windows - код исключения (0xC0000005 и т.п.).
    int exitCode = 0;
    // Почему не запустилась, если started false.
    std::string error;

    bool succeeded() const
    {
        return started && !stopped && exitCode == 0;
    }
};

using ProcessLineHandler = std::function<void(std::string_view line)>;

// Запускает программу и ждёт её конца в потоке вызывающего, поэтому из
// главного потока её не зовут. Вывод stdout и stderr идёт одним потоком, в
// том порядке, в каком программа его пишет: onLine получает строки по мере
// поступления, без перевода строки. stop убивает программу вместе с
// запущенными ею (на Windows - всё дерево процессов).
//
// На Windows окно консоли не появляется, даже если программа консольная, а
// вызывающий - оконное приложение.
YQLORA_API ProcessResult
runProcess(const ProcessOptions& options, const ProcessLineHandler& onLine, std::stop_token stop = {});

struct LaunchResult {
    bool started = false;
    // Почему не запустилась, если started false.
    std::string error;
};

// Запускает программу и не ждёт её: она живёт дальше и после выхода
// вызывающего, её ввод и вывод никуда не идут. Аргументы уходят списком, без
// оболочки. Программа без пути ищется в PATH (на Windows - с добавлением .exe).
//
// На Windows окна консоли нет. Пакетные файлы (.bat, .cmd) не запускаются:
// их разбирает cmd.exe по своим правилам, и аргумент с кавычками или
// спецсимволами списком уже не передать.
YQLORA_API LaunchResult launchDetached(const ProcessOptions& options);

} // namespace yqlora::platform
