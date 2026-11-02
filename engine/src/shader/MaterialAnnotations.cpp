#include "yqlora/shader/MaterialAnnotations.hpp"

#include <algorithm>
#include <optional>
#include <set>

namespace yqlora {

namespace {

constexpr std::string_view kColor = "color";
constexpr std::string_view kRange = "range";
constexpr std::string_view kDefault = "default";
constexpr std::string_view kLabel = "label";
constexpr std::string_view kHidden = "hidden";

struct DefaultTextureName {
    std::string_view name;
    DefaultTexture texture;
};

constexpr DefaultTextureName kDefaultTextures[] = {
    {"white", DefaultTexture::White},
    {"black", DefaultTexture::Black},
    {"normal", DefaultTexture::Normal},
};

bool isNumberType(rhi::ValueType type)
{
    return type == rhi::ValueType::Float || type == rhi::ValueType::Int || type == rhi::ValueType::UInt;
}

bool isColorType(rhi::ValueType type)
{
    return type == rhi::ValueType::Vec3 || type == rhi::ValueType::Vec4;
}

// Проверки аргументов одной пометки; ошибка - с её строкой.
class Checker {
public:
    Checker(const Annotation& annotation, const std::string& file, std::vector<ShaderMessage>& errors)
        : m_annotation(annotation)
        , m_file(file)
        , m_errors(errors)
    {
    }

    void fail(const std::string& text) const
    {
        m_errors.push_back({
            .file = m_file,
            .line = m_annotation.line,
            .text = "annotation '" + m_annotation.word + "': " + text,
        });
    }

    bool arguments(std::size_t count) const
    {
        if (m_annotation.arguments.size() != count) {
            fail(
                "takes " + std::to_string(count) + " arguments, " + std::to_string(m_annotation.arguments.size())
                + " given"
            );

            return false;
        }

        return true;
    }

    bool type(bool fits, const std::string& types) const
    {
        if (!fits) {
            fail("only for " + types);
        }

        return fits;
    }

    std::optional<std::vector<float>> numbers() const
    {
        std::vector<float> result;
        for (const AnnotationArgument& argument : m_annotation.arguments) {
            const double* value = std::get_if<double>(&argument);
            if (!value) {
                fail("takes numbers");

                return std::nullopt;
            }
            result.push_back(static_cast<float>(*value));
        }

        return result;
    }

    const std::string* string() const
    {
        const std::string* text = m_annotation.arguments.size() == 1
                                      ? std::get_if<std::string>(&m_annotation.arguments.front())
                                      : nullptr;
        if (!text) {
            fail("takes one string in quotes");
        }

        return text;
    }

    const AnnotationWord* word() const
    {
        return m_annotation.arguments.size() == 1 ? std::get_if<AnnotationWord>(&m_annotation.arguments.front())
                                                  : nullptr;
    }

private:
    const Annotation& m_annotation;
    const std::string& m_file;
    std::vector<ShaderMessage>& m_errors;
};

// label и hidden есть у всего. false - пометка не из них.
bool applyCommon(const Annotation& annotation, const Checker& check, std::string& label, bool& hidden)
{
    if (annotation.word == kLabel) {
        if (const std::string* text = check.string()) {
            label = *text;
        }

        return true;
    }
    if (annotation.word == kHidden) {
        hidden = check.arguments(0) || hidden;

        return true;
    }

    return false;
}

void applyRange(const Checker& check, MaterialField& field)
{
    if (!check.type(isNumberType(field.type), "float, int and uint") || !check.arguments(2)) {
        return;
    }
    const auto values = check.numbers();
    if (!values) {
        return;
    }
    if ((*values)[0] >= (*values)[1]) {
        check.fail("the minimum must be less than the maximum");

        return;
    }
    field.min = (*values)[0];
    field.max = (*values)[1];
}

void applyDefault(const Checker& check, MaterialField& field)
{
    const std::size_t count = rhi::componentCount(field.type);
    if (!check.type(count > 0, "numbers, vectors and matrices") || !check.arguments(count)) {
        return;
    }
    if (const auto values = check.numbers()) {
        field.defaultValue = *values;
    }
}

// Каждую пометку объявления - один раз; повтор - ошибка.
template <typename Apply>
void forEachOnce(
    const AnnotatedDeclaration& declaration,
    const std::string& file,
    std::vector<ShaderMessage>& errors,
    Apply&& apply
)
{
    std::set<std::string, std::less<>> seen;
    for (const Annotation& annotation : declaration.annotations) {
        const Checker check(annotation, file, errors);
        if (!seen.insert(annotation.word).second) {
            check.fail("given twice for '" + declaration.name + "'");
            continue;
        }
        apply(annotation, check);
    }
}

} // namespace

void applyAnnotations(
    const AnnotatedDeclaration& declaration,
    MaterialField& field,
    const std::string& file,
    std::vector<ShaderMessage>& errors
)
{
    forEachOnce(declaration, file, errors, [&](const Annotation& annotation, const Checker& check) {
        if (annotation.word == kColor) {
            if (check.type(isColorType(field.type), "vec3 and vec4") && check.arguments(0)) {
                field.color = true;
            }
        } else if (annotation.word == kRange) {
            applyRange(check, field);
        } else if (annotation.word == kDefault) {
            applyDefault(check, field);
        } else if (!applyCommon(annotation, check, field.label, field.hidden)) {
            check.fail("unknown annotation");
        }
    });
}

void applyAnnotations(
    const AnnotatedDeclaration& declaration,
    MaterialTexture& texture,
    const std::string& file,
    std::vector<ShaderMessage>& errors
)
{
    forEachOnce(declaration, file, errors, [&](const Annotation& annotation, const Checker& check) {
        if (annotation.word == kDefault) {
            const AnnotationWord* word = check.word();
            const auto found = word ? std::ranges::find(kDefaultTextures, word->text, &DefaultTextureName::name)
                                    : std::ranges::end(kDefaultTextures);
            if (found == std::ranges::end(kDefaultTextures)) {
                check.fail("a texture takes white, black or normal");
            } else {
                texture.defaultTexture = found->texture;
            }
        } else if (!applyCommon(annotation, check, texture.label, texture.hidden)) {
            check.fail("unknown annotation");
        }
    });
}

} // namespace yqlora
