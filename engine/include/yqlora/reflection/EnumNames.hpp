#pragma once

#include <cstddef>
#include <iterator>
#include <optional>
#include <string_view>
#include <type_traits>

namespace yqlora {

// Имена значений перечисления E по порядку значений (0, 1, ...): одна
// таблица для файлов сцен и настроек, .meta, редактора и лога. Задаётся
// рядом с перечислением макросом YQLORA_ENUM_NAMES; свойство с таким
// перечислением получает имена само.
template <typename E>
struct EnumNames;

template <typename E>
concept NamedEnum = std::is_enum_v<E> && requires { EnumNames<E>::names; };

// Пусто, если значения нет в таблице.
template <NamedEnum E>
constexpr std::string_view enumName(E value)
{
    const auto index = static_cast<std::size_t>(value);

    return index < std::size(EnumNames<E>::names) ? EnumNames<E>::names[index] : std::string_view();
}

template <NamedEnum E>
constexpr std::optional<E> enumFromName(std::string_view name)
{
    for (std::size_t i = 0; i < std::size(EnumNames<E>::names); ++i) {
        if (EnumNames<E>::names[i] == name) {
            return static_cast<E>(i);
        }
    }

    return std::nullopt;
}

} // namespace yqlora

// Вне пространств имён, после объявления перечисления, как YQLORA_RECORD:
//   YQLORA_ENUM_NAMES(yqlora::CameraClear, "ColorAndDepth", "DepthOnly", "Nothing")
#define YQLORA_ENUM_NAMES(Enum, ...)                                                                                   \
    template <>                                                                                                        \
    struct yqlora::EnumNames<Enum> {                                                                                   \
        static constexpr std::string_view names[] = {__VA_ARGS__};                                                     \
    };
