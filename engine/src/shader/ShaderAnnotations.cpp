#include "yqlora/shader/ShaderAnnotations.hpp"

#include "yqlora/shader/ShaderProfile.hpp"

#include <cctype>
#include <charconv>
#include <optional>
#include <regex>

namespace yqlora {

namespace {

constexpr std::string_view kAnnotationStart = "//@";
constexpr std::string_view kComment = "//";
constexpr char kMaterialBlockName[] = "Material";

bool isWordStart(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isWordChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string_view trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }

    return text;
}

// Разбор текста пометок после "//@" одной строки.
class AnnotationReader {
public:
    AnnotationReader(std::string_view text, const std::string& file, int line, std::vector<ShaderMessage>& errors)
        : m_text(text)
        , m_file(file)
        , m_line(line)
        , m_errors(errors)
    {
    }

    std::vector<Annotation> read()
    {
        std::vector<Annotation> result;
        while (skipSpaces(), !atEnd()) {
            std::optional<Annotation> annotation = readAnnotation();
            if (!annotation) {
                return {};
            }
            result.push_back(std::move(*annotation));
        }

        return result;
    }

private:
    bool atEnd() const
    {
        return m_at >= m_text.size();
    }

    char peek() const
    {
        return atEnd() ? '\0' : m_text[m_at];
    }

    void skipSpaces()
    {
        while (!atEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
            ++m_at;
        }
    }

    void fail(const std::string& text)
    {
        m_errors.push_back({.file = m_file, .line = m_line, .text = "annotation: " + text});
    }

    std::string readWord()
    {
        const std::size_t start = m_at;
        while (!atEnd() && isWordChar(peek())) {
            ++m_at;
        }

        return std::string(m_text.substr(start, m_at - start));
    }

    std::optional<Annotation> readAnnotation()
    {
        if (!isWordStart(peek())) {
            fail("expected a word, found '" + std::string(1, peek()) + "'");

            return std::nullopt;
        }
        Annotation annotation{.word = readWord(), .line = m_line};
        skipSpaces();
        if (peek() != '(') {
            return annotation;
        }
        ++m_at;
        skipSpaces();
        if (peek() == ')') {
            ++m_at;

            return annotation;
        }
        while (true) {
            skipSpaces();
            std::optional<AnnotationArgument> argument = readArgument();
            if (!argument) {
                return std::nullopt;
            }
            annotation.arguments.push_back(std::move(*argument));
            skipSpaces();
            if (peek() == ',') {
                ++m_at;
                continue;
            }
            if (peek() == ')') {
                ++m_at;

                return annotation;
            }
            fail("'" + annotation.word + "': expected ',' or ')'");

            return std::nullopt;
        }
    }

    std::optional<AnnotationArgument> readArgument()
    {
        if (peek() == '"') {
            const std::size_t close = m_text.find('"', m_at + 1);
            if (close == std::string_view::npos) {
                fail("a string without the closing quote");

                return std::nullopt;
            }
            std::string text(m_text.substr(m_at + 1, close - m_at - 1));
            m_at = close + 1;

            return text;
        }
        if (isWordStart(peek())) {
            return AnnotationWord{readWord()};
        }
        double value = 0.0;
        const char* begin = m_text.data() + m_at;
        const auto [end, error] = std::from_chars(begin, m_text.data() + m_text.size(), value);
        if (error != std::errc()) {
            fail("expected a number, a word or a string in quotes");

            return std::nullopt;
        }
        m_at += static_cast<std::size_t>(end - begin);

        return value;
    }

    std::string_view m_text;
    const std::string& m_file;
    int m_line;
    std::vector<ShaderMessage>& m_errors;
    std::size_t m_at = 0;
};

// Имя текстуры набора материала, если строка её объявляет.
std::optional<std::string> textureDeclaration(const std::string& code)
{
    static const std::regex texture(R"(layout\s*\(([^)]*)\)\s*uniform\s+\w*sampler\w*\s+(\w+))");
    static const std::regex set(R"(\bset\s*=\s*(\d+))");
    std::smatch match;
    if (!std::regex_search(code, match, texture)) {
        return std::nullopt;
    }
    const std::string layout = match[1].str();
    std::smatch setMatch;
    if (!std::regex_search(layout, setMatch, set) || std::stoul(setMatch[1].str()) != kMaterialSet) {
        return std::nullopt;
    }

    return match[2].str();
}

// Имя поля, если строка внутри блока Material объявляет поле.
std::optional<std::string> fieldDeclaration(const std::string& code)
{
    static const std::regex field(R"(^\s*\w+\s+(\w+)\s*(\[[^\]]*\])?\s*;)");
    std::smatch match;
    if (std::regex_search(code, match, field)) {
        return match[1].str();
    }

    return std::nullopt;
}

} // namespace

AnnotationParseResult parseShaderAnnotations(std::string_view source, const std::string& file)
{
    static const std::regex materialStart(std::string(R"(\buniform\s+)") + kMaterialBlockName + R"(\b)");

    AnnotationParseResult result;
    std::vector<Annotation> pending;
    int pendingLine = 0;
    bool inMaterial = false;
    int lineNumber = 0;
    std::size_t start = 0;
    while (start <= source.size()) {
        const std::size_t end = std::min(source.find('\n', start), source.size());
        const std::string_view line = source.substr(start, end - start);
        start = end + 1;
        ++lineNumber;

        const std::size_t comment = line.find(kComment);
        const std::string code(trim(line.substr(0, comment)));
        const bool annotated = comment != std::string_view::npos && line.substr(comment).starts_with(kAnnotationStart);
        std::vector<Annotation> annotations;
        if (annotated) {
            annotations = AnnotationReader(
                              line.substr(comment + kAnnotationStart.size()),
                              file,
                              lineNumber,
                              result.errors
            )
                              .read();
        }

        if (code.empty()) {
            if (annotated) {
                if (pending.empty()) {
                    pendingLine = lineNumber;
                }
                pending.insert(pending.end(), annotations.begin(), annotations.end());
            }
            continue;
        }

        std::optional<std::string> name;
        bool texture = false;
        if (inMaterial) {
            name = fieldDeclaration(code);
        } else if ((name = textureDeclaration(code))) {
            texture = true;
        }
        if (name) {
            AnnotatedDeclaration declaration{.name = *name, .texture = texture, .line = lineNumber};
            declaration.annotations = std::move(pending);
            declaration.annotations.insert(declaration.annotations.end(), annotations.begin(), annotations.end());
            pending.clear();
            if (!declaration.annotations.empty()) {
                result.declarations.push_back(std::move(declaration));
            }
        } else {
            if (!pending.empty()) {
                result.errors.push_back({
                    .file = file,
                    .line = pendingLine,
                    .text = "annotation: no field of the Material block or material texture follows it",
                });
                pending.clear();
            }
            if (annotated) {
                result.errors.push_back({
                    .file = file,
                    .line = lineNumber,
                    .text = "annotation: the line declares no field of the Material block or material texture",
                });
            }
        }
        if (std::regex_search(code, materialStart)) {
            inMaterial = true;
        }
        if (inMaterial && code.find('}') != std::string::npos) {
            inMaterial = false;
        }
    }
    if (!pending.empty()) {
        result.errors.push_back({
            .file = file,
            .line = pendingLine,
            .text = "annotation: no field of the Material block or material texture follows it",
        });
    }

    return result;
}

} // namespace yqlora
