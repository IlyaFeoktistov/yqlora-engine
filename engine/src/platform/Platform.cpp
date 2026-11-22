#include "yqlora/platform/Platform.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// После windows.h: ShellExecuteW.
#include <shellapi.h>

#include <cstdio>
#endif

namespace yqlora::platform {

namespace {

constexpr double kNanosecondsPerSecond = 1e9;

} // namespace

void openConsole()
{
#ifdef _WIN32
    if (!AttachConsole(ATTACH_PARENT_PROCESS) && !AllocConsole()) {
        return;
    }
    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    // Лог в UTF-8, иначе кириллица в путях превратится в кракозябры.
    SetConsoleOutputCP(CP_UTF8);
#endif
}

bool initialize()
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        log::error("SDL_Init failed: {}", SDL_GetError());

        return false;
    }
    log::info(
        "Platform: SDL {}.{}.{}, video driver '{}'",
        SDL_MAJOR_VERSION,
        SDL_MINOR_VERSION,
        SDL_MICRO_VERSION,
        SDL_GetCurrentVideoDriver()
    );

    return true;
}

void shutdown()
{
    SDL_Quit();
}

std::filesystem::path executableDir()
{
    const char* base = SDL_GetBasePath();

    return base ? fromUtf8(base) : std::filesystem::current_path();
}

std::filesystem::path userDataPath(std::string_view organization, std::string_view application)
{
    char* path = SDL_GetPrefPath(std::string(organization).c_str(), std::string(application).c_str());
    if (!path) {
        log::warn("SDL_GetPrefPath failed: {}, using current directory", SDL_GetError());

        return std::filesystem::current_path();
    }
    std::filesystem::path result = fromUtf8(path);
    SDL_free(path);

    return result;
}

std::filesystem::path documentsPath()
{
    if (const char* documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS)) {
        return fromUtf8(documents);
    }
    if (const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME)) {
        return fromUtf8(home);
    }

    return std::filesystem::current_path();
}

std::vector<std::string> preferredLanguages()
{
    std::vector<std::string> result;
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    if (!locales) {
        return result;
    }
    for (int i = 0; i < count; ++i) {
        if (locales[i] && locales[i]->language) {
            result.emplace_back(locales[i]->language);
        }
    }
    SDL_free(locales);

    return result;
}

double timeSeconds()
{
    return static_cast<double>(SDL_GetTicksNS()) / kNanosecondsPerSecond;
}

bool openWithSystem(const std::filesystem::path& path)
{
#ifdef _WIN32
    // ShellExecuteW берёт путь как есть, без URL и его кодирования.
    constexpr INT_PTR kFirstSuccess = 32;
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL)
    );
    if (result <= kFirstSuccess) {
        log::error("Cannot open '{}'", toUtf8(path));

        return false;
    }

    return true;
#else
    // URL file://: байты пути вне безопасных символов - %XX.
    constexpr std::string_view kSafe = "/-_.~";
    constexpr const char* kHex = "0123456789ABCDEF";
    constexpr unsigned kHighNibble = 4;
    constexpr unsigned kLowNibbleMask = 0x0F;
    std::string url = "file://";
    for (const char c : toUtf8(path)) {
        const auto byte = static_cast<unsigned char>(c);
        // isalnum в локали "C" - только латиница и цифры.
        if (std::isalnum(byte) != 0 || kSafe.find(c) != std::string_view::npos) {
            url += c;
        } else {
            url += '%';
            url += kHex[byte >> kHighNibble];
            url += kHex[byte & kLowNibbleMask];
        }
    }
    if (!SDL_OpenURL(url.c_str())) {
        log::error("Cannot open '{}': {}", toUtf8(path), SDL_GetError());

        return false;
    }

    return true;
#endif
}

bool openUrl(const std::string& url)
{
    if (!SDL_OpenURL(url.c_str())) {
        log::error("Cannot open '{}': {}", url, SDL_GetError());

        return false;
    }

    return true;
}

std::optional<std::filesystem::path> asciiPath(const std::filesystem::path& path)
{
#ifdef _WIN32
    constexpr wchar_t kLastAscii = 0x7F;
    const auto ascii = [](const std::wstring& text) {
        return std::ranges::all_of(text, [](wchar_t c) {
            return c <= kLastAscii;
        });
    };
    const std::wstring wide = path.wstring();
    if (ascii(wide)) {
        return path;
    }
    const DWORD length = GetShortPathNameW(wide.c_str(), nullptr, 0);
    if (length == 0) {
        return std::nullopt;
    }
    std::wstring result(length, L'\0');
    const DWORD written = GetShortPathNameW(wide.c_str(), result.data(), length);
    result.resize(written < length ? written : 0);
    if (result.empty() || !ascii(result)) {
        return std::nullopt;
    }

    return std::filesystem::path(result);
#else
    return path;
#endif
}

} // namespace yqlora::platform
