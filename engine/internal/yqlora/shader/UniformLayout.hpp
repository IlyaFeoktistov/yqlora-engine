#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/ShaderReflection.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace yqlora {

struct UniformMember {
    std::string_view name;
    std::size_t offset = 0;
    std::size_t size = 0;
    // Шаг элемента массива; 0 - не массив.
    std::size_t arrayStride = 0;
};

// Шаг элемента, если T - массив C++, иначе 0.
template <typename T>
constexpr std::size_t uniformArrayStride()
{
    if constexpr (std::is_array_v<T>) {
        return sizeof(std::remove_extent_t<T>);
    } else {
        return 0;
    }
}

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
        #member, offsetof(Struct, member), sizeof(Struct::member),                                                     \
            ::yqlora::uniformArrayStride<decltype(Struct::member)>()                                                   \
    }

// Ошибки по одной на расхождение; пусто - блок шейдера совпадает со
// структурой. Шейдер может объявить не все поля и вовсе не объявить блок,
// но каждое его поле должно быть в структуре на том же месте и того же
// размера, а у массива ещё и с тем же шагом элемента.
YQLORA_API std::vector<std::string> checkUniformLayout(
    const rhi::ShaderReflection& reflection,
    const UniformLayout& layout
);

} // namespace yqlora
