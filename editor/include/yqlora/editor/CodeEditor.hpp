#pragma once

#include <filesystem>
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
    std::string arguments;
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

} // namespace yqlora::editor
