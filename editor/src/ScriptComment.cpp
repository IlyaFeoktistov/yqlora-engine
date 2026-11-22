#include "yqlora/editor/ScriptComment.hpp"

#include "yqlora/platform/Files.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <vector>

namespace yqlora::editor {

namespace {

constexpr std::string_view kCommentStart = "//";
constexpr std::array<std::string_view, 2> kClassKeywords = {"class", "struct"};
constexpr std::array<std::string_view, 2> kHeaderExtensions = {".hpp", ".h"};

std::string_view trimmed(std::string_view text)
{
    const auto space = [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    };
    while (!text.empty() && space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && space(text.back())) {
        text.remove_suffix(1);
    }

    return text;
}

bool isWordChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Слова строки: идентификаторы, остальное - разделители.
std::vector<std::string_view> words(std::string_view line)
{
    std::vector<std::string_view> result;
    std::size_t i = 0;
    while (i < line.size()) {
        if (!isWordChar(line[i])) {
            ++i;
            continue;
        }
        const std::size_t start = i;
        while (i < line.size() && isWordChar(line[i])) {
            ++i;
        }
        result.push_back(line.substr(start, i - start));
    }

    return result;
}

// Строка объявляет класс className: class или struct, затем, может быть,
// макрос экспорта, затем имя. Предварительное объявление (с ;) не считается.
// TODO: разбор по строкам, а не по C++: класс, объявленный макросом или
// с переносом после class, не найдётся. Надёжный путь - .description().
bool declaresClass(std::string_view line, std::string_view className)
{
    const std::vector<std::string_view> found = words(line);
    if (found.size() < 2 || std::ranges::find(kClassKeywords, found[0]) == kClassKeywords.end()
        || line.find(';') != std::string_view::npos) {
        return false;
    }
    const std::size_t nameAt = found[1] == className ? 1 : 2;

    return nameAt < found.size() && found[nameAt] == className;
}

std::vector<std::string_view> lines(std::string_view text)
{
    std::vector<std::string_view> result;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        result.push_back(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
    }

    return result;
}

} // namespace

std::string classComment(std::string_view source, std::string_view className)
{
    const std::vector<std::string_view> all = lines(source);
    std::optional<std::size_t> declaration;
    for (std::size_t i = 0; i < all.size() && !declaration; ++i) {
        if (declaresClass(trimmed(all[i]), className)) {
            declaration = i;
        }
    }
    if (!declaration) {
        return {};
    }
    std::size_t first = *declaration;
    while (first > 0 && trimmed(all[first - 1]).starts_with(kCommentStart)) {
        --first;
    }
    std::string result;
    for (std::size_t i = first; i < *declaration; ++i) {
        std::string_view text = trimmed(all[i]);
        while (text.starts_with('/')) {
            text.remove_prefix(1);
        }
        text = trimmed(text);
        if (text.empty()) {
            result += result.empty() || result.ends_with('\n') ? "" : "\n";
            continue;
        }
        if (!result.empty() && !result.ends_with('\n')) {
            result += ' ';
        }
        result += text;
    }

    return std::string(trimmed(result));
}

std::string scriptComment(const std::filesystem::path& sourceFile, std::string_view className)
{
    std::vector<std::filesystem::path> candidates;
    for (const std::string_view extension : kHeaderExtensions) {
        candidates.push_back(std::filesystem::path(sourceFile).replace_extension(extension));
    }
    candidates.push_back(sourceFile);
    for (const std::filesystem::path& candidate : candidates) {
        if (const auto text = platform::readFile(candidate)) {
            if (std::string comment = classComment(*text, className); !comment.empty()) {
                return comment;
            }
        }
    }

    return {};
}

} // namespace yqlora::editor
