#pragma once

#include "yqlora/editor/CompilerDiagnostics.hpp"
#include "yqlora/platform/Process.hpp"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora::editor {

// Программа, в которой человек правит код. Привычка человека, а не проекта:
// хранится в настройках редактора.
struct CodeEditorSetting {
    // Пусто - файл открывает система, без строки.
    std::filesystem::path program;
    // Шаблон аргументов: слова через пробел, "..." внутри слова - кусок с
    // пробелами. Метки {file}, {line}, {column}, {project} заменяются местом,
    // {{ и }} - сами скобки.
    std::string arguments = "{file}";
};

// Место в файле. Строка и столбец с 1; 0 - не названы, подставляется 1.
struct CodeLocation {
    std::filesystem::path file;
    int line = 0;
    int column = 0;
};

// Готовая строка для известного редактора: заполняет программу и шаблон.
// Программа - имя для поиска в PATH; лежит в другом месте - путь выбирают.
struct CodeEditorPreset {
    std::string_view name;
    std::string_view program;
    std::string_view arguments;
};

std::span<const CodeEditorPreset> codeEditorPresets();

struct CodeEditorArguments {
    std::vector<std::string> arguments;
    // Ключ перевода; пусто - шаблон верен. detail - к чему он относится
    // (неизвестная метка, хвост без закрывающей скобки), без перевода.
    std::string error;
    std::string detail;

    bool ok() const
    {
        return error.empty();
    }
};

// Аргументы по шаблону. Место подставляется после разбиения на слова, поэтому
// путь с пробелами и кавычками остаётся одним аргументом. Слово, которое
// после подстановки пусто ({project} без проекта), пропускается; "" остаётся
// пустым аргументом. Ошибка шаблона (неизвестная метка, незакрытая кавычка
// или скобка, нет {file}) - ошибка настройки, аргументов тогда нет.
CodeEditorArguments expandCodeEditorArguments(
    std::string_view arguments,
    const CodeLocation& location,
    const std::filesystem::path& project
);

// Файл правят в программе для кода, а не открывают системой: исходники C++,
// шейдеры и подключаемые в них файлы.
bool isCodeFile(const std::filesystem::path& path);

// Место, которое называет строка вывода компилятора или шейдера. Относительный
// путь считается от directory. nullopt - места в строке нет или файла нет на
// диске (встроенный файл движка "engine:...").
std::optional<CodeLocation> diagnosticLocation(
    const CompilerDiagnostic& diagnostic,
    const std::filesystem::path& directory
);

// Как открыть место: программой из настройки, системой (программа не
// задана, строки она не знает) или никак (шаблон с ошибкой).
struct CodeEditorOpening {
    enum class Way {
        Program,
        System,
        Error,
    };

    Way way = Way::System;
    // Way::Program: запуск; рабочая папка - папка проекта.
    platform::ProcessOptions process;
    // Way::System: файл без строки.
    std::filesystem::path file;
    // Way::Error: как в CodeEditorArguments.
    std::string error;
    std::string detail;
};

CodeEditorOpening codeEditorOpening(
    const CodeEditorSetting& setting,
    const CodeLocation& location,
    const std::filesystem::path& project
);

// Открывает место по codeEditorOpening; что не вышло - в лог. false, если не вышло.
bool openInCodeEditor(
    const CodeEditorSetting& setting,
    const CodeLocation& location,
    const std::filesystem::path& project
);

} // namespace yqlora::editor
