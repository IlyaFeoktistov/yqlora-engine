#include "yqlora/platform/Process.hpp"

#include "yqlora/core/Utf8Path.hpp"

#include <array>
#include <atomic>
#include <cstddef>

#if defined(_WIN32)
// Стандартная библиотека MinGW может уже определить их.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cwctype>
#else
#include <SDL3/SDL.h>

#include <chrono>
#include <thread>
#endif

namespace yqlora::platform {

namespace {

constexpr std::size_t kReadChunk = 4096;

// Режет поток байтов на строки; "\r\n" и "\n" одинаково.
class LineSplitter {
public:
    explicit LineSplitter(const ProcessLineHandler& onLine)
        : m_onLine(onLine)
    {
    }

    void feed(const char* data, std::size_t size)
    {
        for (std::size_t i = 0; i < size; ++i) {
            if (data[i] == '\n') {
                emit();
            } else {
                m_line += data[i];
            }
        }
    }

    // Последняя строка без перевода строки в конце.
    void finish()
    {
        if (!m_line.empty()) {
            emit();
        }
    }

private:
    void emit()
    {
        if (!m_line.empty() && m_line.back() == '\r') {
            m_line.pop_back();
        }
        if (m_onLine) {
            m_onLine(m_line);
        }
        m_line.clear();
    }

    const ProcessLineHandler& m_onLine;
    std::string m_line;
};

#if defined(_WIN32)

std::wstring toWide(std::string_view utf8)
{
    return fromUtf8(utf8).wstring();
}

// Аргумент командной строки по правилам CommandLineToArgvW: кавычки, если
// есть пробелы или кавычки, обратные косые перед кавычкой удваиваются.
std::wstring quoteArgument(const std::wstring& argument)
{
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return argument;
    }
    std::wstring quoted = L"\"";
    for (auto it = argument.begin();; ++it) {
        std::size_t backslashes = 0;
        while (it != argument.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }
        if (it == argument.end()) {
            quoted.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
        } else {
            quoted.append(backslashes, L'\\');
        }
        quoted.push_back(*it);
    }
    quoted.push_back(L'"');

    return quoted;
}

std::string lastError()
{
    return "Windows error " + std::to_string(GetLastError());
}

// Закрывает описатель при выходе из области.
class Handle {
public:
    Handle() = default;

    explicit Handle(HANDLE handle)
        : m_handle(handle)
    {
    }

    ~Handle()
    {
        reset();
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    HANDLE get() const
    {
        return m_handle;
    }

    HANDLE* out()
    {
        reset();

        return &m_handle;
    }

    void reset()
    {
        if (m_handle && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
        }
        m_handle = nullptr;
    }

private:
    HANDLE m_handle = nullptr;
};

// Код, с которым завершается остановленное дерево процессов.
constexpr UINT kStoppedExitCode = 1;

ProcessResult runPlatform(const ProcessOptions& options, LineSplitter& lines, std::stop_token stop)
{
    ProcessResult result;

    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength = sizeof(inheritable);
    inheritable.bInheritHandle = TRUE;
    Handle readPipe;
    Handle writePipe;
    if (!CreatePipe(readPipe.out(), writePipe.out(), &inheritable, 0)) {
        result.error = lastError();

        return result;
    }
    // Конец для чтения остаётся только у нас: иначе труба не закроется, пока жив потомок.
    SetHandleInformation(readPipe.get(), HANDLE_FLAG_INHERIT, 0);
    Handle input(
        CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING, 0, nullptr)
    );

    // Задание убивает всё дерево (cmake, ninja, компилятор) при остановке и
    // при выходе редактора.
    Handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits));

    std::wstring commandLine = quoteArgument(options.program.wstring());
    for (const std::string& argument : options.arguments) {
        commandLine += L' ';
        commandLine += quoteArgument(toWide(argument));
    }
    const std::wstring directory = options.workingDirectory.wstring();

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input.get();
    startup.hStdOutput = writePipe.get();
    startup.hStdError = writePipe.get();
    PROCESS_INFORMATION info{};
    // Приостановлен, пока не вошёл в задание: иначе успел бы породить потомков вне его.
    if (!CreateProcessW(
            nullptr,
            commandLine.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,
            nullptr,
            directory.empty() ? nullptr : directory.c_str(),
            &startup,
            &info
        )) {
        result.error = lastError();

        return result;
    }
    Handle process(info.hProcess);
    Handle thread(info.hThread);
    AssignProcessToJobObject(job.get(), process.get());
    ResumeThread(thread.get());
    result.started = true;
    // Наша копия конца для записи мешала бы увидеть конец вывода.
    writePipe.reset();
    input.reset();

    std::atomic<bool> stopped = false;
    {
        const std::stop_callback onStop(stop, [&] {
            stopped = true;
            TerminateJobObject(job.get(), kStoppedExitCode);
        });
        std::array<char, kReadChunk> buffer{};
        DWORD read = 0;
        while (ReadFile(readPipe.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read > 0) {
            lines.feed(buffer.data(), read);
        }
    }
    WaitForSingleObject(process.get(), INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.get(), &exitCode);
    result.exitCode = static_cast<int>(exitCode);
    result.stopped = stopped;

    return result;
}

bool isBatchFile(const std::filesystem::path& program)
{
    std::wstring extension = program.extension().wstring();
    for (wchar_t& c : extension) {
        c = static_cast<wchar_t>(std::towlower(c));
    }

    return extension == L".bat" || extension == L".cmd";
}

LaunchResult launchPlatform(const ProcessOptions& options)
{
    LaunchResult result;
    if (isBatchFile(options.program)) {
        result.error = "Batch files are not launched: choose the program itself (.exe)";

        return result;
    }

    std::wstring commandLine = quoteArgument(options.program.wstring());
    for (const std::string& argument : options.arguments) {
        commandLine += L' ';
        commandLine += quoteArgument(toWide(argument));
    }
    const std::wstring directory = options.workingDirectory.wstring();

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    // Без наследования описателей: открытые файлы и трубы вызывающего
    // не остаются жить в чужой программе.
    if (!CreateProcessW(
            nullptr,
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_PROCESS_GROUP,
            nullptr,
            directory.empty() ? nullptr : directory.c_str(),
            &startup,
            &info
        )) {
        result.error = lastError();

        return result;
    }
    CloseHandle(info.hThread);
    CloseHandle(info.hProcess);
    result.started = true;

    return result;
}

#else

// SDL отдаёт вывод без блокировки: пока его нет, ждём столько и проверяем остановку.
constexpr std::chrono::milliseconds kPollInterval{10};

ProcessResult runPlatform(const ProcessOptions& options, LineSplitter& lines, std::stop_token stop)
{
    ProcessResult result;

    std::vector<std::string> arguments;
    arguments.push_back(toUtf8(options.program));
    arguments.insert(arguments.end(), options.arguments.begin(), options.arguments.end());
    std::vector<const char*> argv;
    for (const std::string& argument : arguments) {
        argv.push_back(argument.c_str());
    }
    argv.push_back(nullptr);
    const std::string directory = toUtf8(options.workingDirectory);

    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    if (!directory.empty()) {
        SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, directory.c_str());
    }
    SDL_Process* process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process) {
        result.error = SDL_GetError();

        return result;
    }
    result.started = true;

    SDL_IOStream* output = SDL_GetProcessOutput(process);
    std::array<char, kReadChunk> buffer{};
    while (true) {
        const std::size_t read = SDL_ReadIO(output, buffer.data(), buffer.size());
        if (read > 0) {
            lines.feed(buffer.data(), read);
            continue;
        }
        if (SDL_GetIOStatus(output) != SDL_IO_STATUS_NOT_READY) {
            break;
        }
        if (stop.stop_requested()) {
            // Потомки (ninja, компилятор) получат SIGPIPE, как только запишут в закрытую трубу.
            SDL_KillProcess(process, true);
            result.stopped = true;
            break;
        }
        std::this_thread::sleep_for(kPollInterval);
    }
    SDL_WaitProcess(process, true, &result.exitCode);
    SDL_DestroyProcess(process);

    return result;
}

LaunchResult launchPlatform(const ProcessOptions& options)
{
    LaunchResult result;

    std::vector<std::string> arguments;
    arguments.push_back(toUtf8(options.program));
    arguments.insert(arguments.end(), options.arguments.begin(), options.arguments.end());
    std::vector<const char*> argv;
    for (const std::string& argument : arguments) {
        argv.push_back(argument.c_str());
    }
    argv.push_back(nullptr);
    const std::string directory = toUtf8(options.workingDirectory);

    // Фоновый процесс SDL отделяется от терминала (setsid) и усыновляется
    // init, так что зомби после него не остаётся; ввод и вывод - /dev/null.
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    if (!directory.empty()) {
        SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, directory.c_str());
    }
    SDL_Process* process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process) {
        result.error = SDL_GetError();

        return result;
    }
    // Освобождает только описание: сама программа продолжает работать.
    SDL_DestroyProcess(process);
    result.started = true;

    return result;
}

#endif

} // namespace

ProcessResult runProcess(const ProcessOptions& options, const ProcessLineHandler& onLine, std::stop_token stop)
{
    LineSplitter lines(onLine);
    ProcessResult result = runPlatform(options, lines, stop);
    lines.finish();

    return result;
}

LaunchResult launchDetached(const ProcessOptions& options)
{
    return launchPlatform(options);
}

} // namespace yqlora::platform
