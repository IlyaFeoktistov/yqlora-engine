#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/shader/ShaderCompiler.hpp"

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace yqlora {

// Аргумент пометки: число, строка в кавычках или слово (default(white)).
struct AnnotationWord {
    std::string text{};

    bool operator==(const AnnotationWord&) const = default;
};

using AnnotationArgument = std::variant<double, std::string, AnnotationWord>;

// Одна пометка: слово и необязательные аргументы в скобках.
struct Annotation {
    std::string word{};
    std::vector<AnnotationArgument> arguments{};
    int line = 0;
};

// Объявление, к которому относятся пометки: поле блока Material или
// текстура набора материала.
struct AnnotatedDeclaration {
    std::string name{};
    bool texture = false;
    int line = 0;
    std::vector<Annotation> annotations{};
};

struct AnnotationParseResult {
    std::vector<AnnotatedDeclaration> declarations{};
    std::vector<ShaderMessage> errors{};
};

// Пометки инспектора материала в тексте шейдера (см. materials.md, "Файлы
// шейдеров"): строчный комментарий "//@" и пометки через пробел. Пометки на
// отдельных строках относятся к следующему объявлению (между ними могут
// быть пустые строки и обычные комментарии), "//@" в конце строки
// объявления - к нему же. Читается только сам файл шейдера, не подключения:
// блок материала и текстуры объявляет он. Смысл пометок и их типы
// проверяет applyAnnotations по отражению.
YQLORA_API AnnotationParseResult parseShaderAnnotations(std::string_view source, const std::string& file);

} // namespace yqlora
