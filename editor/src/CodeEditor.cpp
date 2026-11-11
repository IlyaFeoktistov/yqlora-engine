#include "yqlora/editor/CodeEditor.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/platform/Platform.hpp"
#include "yqlora/project/Project.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <system_error>

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

// Подключаемые файлы шейдеров (#include): имя расширения выбирает автор,
// это - принятое для GLSL.
constexpr std::string_view kShaderIncludeExtension = ".glsl";

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

bool isCodeFile(const std::filesystem::path& path)
{
    const std::filesystem::path extension = path.extension();

    return Project::isSource(path) || extension == kShaderFileExtension || extension == kShaderIncludeExtension;
}

std::optional<CodeLocation> diagnosticLocation(
    const CompilerDiagnostic& diagnostic,
    const std::filesystem::path& directory
)
{
    if (diagnostic.file.empty()) {
        return std::nullopt;
    }
    std::filesystem::path file = fromUtf8(diagnostic.file);
    if (file.is_relative()) {
        file = directory / file;
    }
    file = file.lexically_normal();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) {
        return std::nullopt;
    }

    return CodeLocation{.file = std::move(file), .line = diagnostic.line, .column = diagnostic.column};
}

CodeEditorOpening codeEditorOpening(
    const CodeEditorSetting& setting,
    const CodeLocation& location,
    const std::filesystem::path& project
)
{
    CodeEditorOpening opening;
    if (setting.program.empty()) {
        opening.way = CodeEditorOpening::Way::System;
        opening.file = location.file;

        return opening;
    }
    CodeEditorArguments arguments = expandCodeEditorArguments(setting.arguments, location, project);
    if (!arguments.ok()) {
        opening.way = CodeEditorOpening::Way::Error;
        opening.error = std::move(arguments.error);
        opening.detail = std::move(arguments.detail);

        return opening;
    }
    opening.way = CodeEditorOpening::Way::Program;
    opening.process = {
        .program = setting.program,
        .arguments = std::move(arguments.arguments),
        .workingDirectory = project,
    };

    return opening;
}

bool openInCodeEditor(
    const CodeEditorSetting& setting,
    const CodeLocation& location,
    const std::filesystem::path& project
)
{
    const CodeEditorOpening opening = codeEditorOpening(setting, location, project);
    switch (opening.way) {
    case CodeEditorOpening::Way::System: return platform::openWithSystem(opening.file);
    case CodeEditorOpening::Way::Error:
        log::error("Code editor arguments: {} {}", tr(opening.error), opening.detail);

        return false;
    case CodeEditorOpening::Way::Program: break;
    }
    const platform::LaunchResult launched = platform::launchDetached(opening.process);
    if (!launched.started) {
        log::error("Cannot run code editor '{}': {}", toUtf8(opening.process.program), launched.error);
    }

    return launched.started;
}

} // namespace yqlora::editor
