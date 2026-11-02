#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/shader/ShaderAnnotations.hpp"
#include "yqlora/shader/ShaderCompiler.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <string>
#include <vector>

namespace yqlora {

// Смысл пометок (разобранных parseShaderAnnotations) для поля материала:
// color, range, default, label, hidden, каждая по типу поля из отражения.
// Незнакомая пометка, неверные аргументы, пометка не по типу и повтор -
// ошибки в errors с файлом file и строкой пометки; такая пометка не
// применяется.
YQLORA_API void applyAnnotations(
    const AnnotatedDeclaration& declaration,
    MaterialField& field,
    const std::string& file,
    std::vector<ShaderMessage>& errors
);

// То же для текстуры материала: default(white | black | normal), label, hidden.
YQLORA_API void applyAnnotations(
    const AnnotatedDeclaration& declaration,
    MaterialTexture& texture,
    const std::string& file,
    std::vector<ShaderMessage>& errors
);

} // namespace yqlora
