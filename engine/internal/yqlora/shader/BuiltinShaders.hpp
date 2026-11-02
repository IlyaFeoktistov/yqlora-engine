#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <span>
#include <string_view>

namespace yqlora {

// Шейдер, встроенный в движок: файл из engine/shaders с постоянным ID
// (см. BuiltinResources.hpp). Сами данные держит ResourceCache.
struct BuiltinShader {
    ResourceId id;
    std::string_view name;
    // Путь внутри engine/shaders.
    std::string_view file;
};

YQLORA_API std::span<const BuiltinShader> builtinShaders();

// Компилирует встроенный шейдер. Его ошибка - ошибка сборки движка, поэтому
// std::runtime_error со всеми сообщениями.
YQLORA_API ShaderData makeBuiltinShader(const BuiltinShader& shader);

} // namespace yqlora
