#include "yqlora/editor/CodeEditor.hpp"

#include "yqlora/core/Utf8Path.hpp"

#include <algorithm>
#include <array>
#include <optional>

namespace yqlora::editor {

namespace {

constexpr std::string_view kFilePlaceholder = "file";
constexpr std::string_view kLinePlaceholder = "line";
constexpr std::string_view kColumnPlaceholder = "column";
constexpr std::string_view kProjectPlaceholder = "project";

constexpr const char* kUnknownPlaceholderError = "editor.codeEditor.unknownPlaceholder";
constexpr const char* kUnclosedBraceError = "editor.codeEditor.unclosedBrace";
constexpr const char* kStrayBraceError = "editor.codeEditor.strayBrace";
constexpr const char* kUnclosedQuoteError = "editor.codeEditor.unclosedQuote";
constexpr const char* kNoFileError = "editor.codeEditor.noFile";

constexpr char kOpenBrace = '{';
constexpr char kCloseBrace = '}';
constexpr char kQuote = '"';

#if defined(_WIN32)
constexpr std::array kPresets{
    CodeEditorPreset{"Visual Studio Code", "Code.exe", "{project} --goto {file}:{line}:{column}"},
    CodeEditorPreset{"CLion", "clion64.exe", "--line {line} --column {column} {file}"},
    CodeEditorPreset{"Sublime Text", "subl.exe", "{file}:{line}:{column}"},
};
#else
constexpr std::array kPresets{
    CodeEditorPreset{"Visual Studio Code", "code", "{project} --goto {file}:{line}:{column}"},
    CodeEditorPreset{"CLion", "clion", "--line {line} --column {column} {file}"},
    CodeEditorPreset{"Sublime Text", "subl", "{file}:{line}:{column}"},
};
#endif

std::optional<std::string> placeholderValue(
    std::string_view name,
    const CodeLocation& location,
    const std::filesystem::path& project
)
{
    if (name == kFilePlaceholder) {
        return toUtf8(location.file);
    }
    if (name == kLinePlaceholder) {
        return std::to_string(std::max(location.line, 1));
    }
    if (name == kColumnPlaceholder) {
        return std::to_string(std::max(location.column, 1));
    }
    if (name == kProjectPlaceholder) {
        return toUtf8(project);
    }

    return std::nullopt;
}

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

CodeEditorArguments failure(const char* error, std::string_view detail)
{
    CodeEditorArguments result;
    result.error = error;
    result.detail = detail;

    return result;
}

} // namespace

std::span<const CodeEditorPreset> codeEditorPresets()
{
    return kPresets;
}

CodeEditorArguments expandCodeEditorArguments(
    std::string_view arguments,
    const CodeLocation& location,
    const std::filesystem::path& project
)
{
    CodeEditorArguments result;
    std::string word;
    bool inWord = false;
    // Слово с кавычками остаётся, даже пустое: "" - намеренно пустой аргумент.
    bool quoted = false;
    bool inQuotes = false;
    bool hasFile = false;
    const auto flush = [&] {
        if (inWord && (!word.empty() || quoted)) {
            result.arguments.push_back(std::move(word));
        }
        word.clear();
        inWord = false;
        quoted = false;
    };

    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const char c = arguments[i];
        if (!inQuotes && isSpace(c)) {
            flush();
            continue;
        }
        inWord = true;
        const bool doubled = i + 1 < arguments.size() && arguments[i + 1] == c;
        if (c == kQuote) {
            inQuotes = !inQuotes;
            quoted = true;
        } else if (c == kOpenBrace && doubled) {
            word += c;
            ++i;
        } else if (c == kOpenBrace) {
            const std::size_t close = arguments.find(kCloseBrace, i + 1);
            if (close == std::string_view::npos) {
                return failure(kUnclosedBraceError, arguments.substr(i));
            }
            const std::string_view name = arguments.substr(i + 1, close - i - 1);
            const auto value = placeholderValue(name, location, project);
            if (!value) {
                return failure(kUnknownPlaceholderError, arguments.substr(i, close - i + 1));
            }
            hasFile = hasFile || name == kFilePlaceholder;
            word += *value;
            i = close;
        } else if (c == kCloseBrace && doubled) {
            word += c;
            ++i;
        } else if (c == kCloseBrace) {
            return failure(kStrayBraceError, arguments.substr(i));
        } else {
            word += c;
        }
    }
    if (inQuotes) {
        return failure(kUnclosedQuoteError, {});
    }
    flush();
    if (!hasFile) {
        return failure(kNoFileError, {});
    }

    return result;
}

} // namespace yqlora::editor
