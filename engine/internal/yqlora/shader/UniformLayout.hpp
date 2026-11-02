#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/ShaderReflection.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

struct UniformMember {
    std::string_view name;
    std::size_t offset = 0;
    std::size_t size = 0;
};

// Раскладка структуры C++, которая повторяет uniform-блок std140. Шейдеру
// не верят на слово: при загрузке его блок сверяется с ней по каждому полю.
struct UniformLayout {
    std::string_view name;
    rhi::BindingSlot slot{};
    std::size_t size = 0;
    std::vector<UniformMember> members{};
};

// Поле структуры по имени: имя в C++ и в шейдере одно и то же.
#define YQLORA_UNIFORM_MEMBER(Struct, member)                                                                          \
    ::yqlora::UniformMember                                                                                            \
    {                                                                                                                  \
        #member, offsetof(Struct, member), sizeof(Struct::member)                                                      \
    }

// Ошибки по одной на расхождение; пусто - блок шейдера совпадает со
// структурой. Шейдер может объявить не все поля и вовсе не объявить блок,
// но каждое его поле должно быть в структуре на том же месте и того же размера.
YQLORA_API std::vector<std::string> checkUniformLayout(
    const rhi::ShaderReflection& reflection,
    const UniformLayout& layout
);

} // namespace yqlora
