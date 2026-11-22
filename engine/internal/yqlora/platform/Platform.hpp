#pragma once

#include "yqlora/core/Export.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora::platform {

// Windows: окно консоли для лога. Программа собрана без консоли, поэтому
// по умолчанию её нет. Если программу запустили из терминала, лог идёт в него,
// иначе открывается новое окно. На других системах ничего не делает:
// вывод и так попадает в терминал, из которого запустили программу.
YQLORA_API void openConsole();

// Инициализация оконной подсистемы (SDL). Нужна до создания Window.
YQLORA_API bool initialize();
YQLORA_API void shutdown();

// Папка, где лежит исполняемый файл (рядом с ним лежит assets/).
YQLORA_API std::filesystem::path executableDir();

// Папка для пользовательских данных: ~/.local/share/<org>/<app> на Linux,
// %APPDATA%\<org>\<app> на Windows. Создаётся при первом обращении.
YQLORA_API std::filesystem::path userDataPath(std::string_view organization, std::string_view application);

// Папка "Документы" пользователя; если система её не знает, домашняя папка.
YQLORA_API std::filesystem::path documentsPath();

// Языки пользователя в порядке предпочтения, как их отдаёт ОС: "ru_RU", "en"...
YQLORA_API std::vector<std::string> preferredLanguages();

// Открывает файл или папку программой, которую для них назначила система
// (папку - файловым менеджером). false, если не вышло.
YQLORA_API bool openWithSystem(const std::filesystem::path& path);

// Открывает адрес (https://...) браузером системы. false, если не вышло.
YQLORA_API bool openUrl(const std::string& url);

// Путь к существующему файлу или папке, который понимают программы без
// поддержки Unicode в путях (Ninja на Windows). На Windows - сам path, если в
// нём только ASCII, иначе его короткий вариант (8.3); nullopt, если том
// коротких имён не хранит. На других системах путь - байты UTF-8, их
// понимают все: path как есть.
YQLORA_API std::optional<std::filesystem::path> asciiPath(const std::filesystem::path& path);

// Время с момента запуска в секундах (монотонное).
YQLORA_API double timeSeconds();

} // namespace yqlora::platform
