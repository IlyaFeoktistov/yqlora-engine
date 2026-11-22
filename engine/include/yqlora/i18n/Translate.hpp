#pragma once

#include "yqlora/core/Export.hpp"

#include <format>
#include <string>
#include <string_view>

namespace yqlora {

// Перевод текста интерфейса по ключу на текущий язык. Если перевода нет,
// возвращается сам ключ. Берёт локализацию, которую включило приложение:
// это общая служба движка, как и лог, и живёт в библиотеке движка в одном экземпляре.
YQLORA_API std::string tr(std::string_view key);

// Перевод с подстановкой: "Только на узлах типа {}" -> "Только на узлах типа
// Node3D". Место значения в фразе решает перевод, а не код, поэтому порядок
// слов в языках может быть разным ({0}, {1} - по номеру). Перевод с неверными
// скобками показывается как есть, без подстановки.
template <typename... Args>
std::string trFormat(std::string_view key, const Args&... args)
{
    const std::string text = tr(key);
    try {
        return std::vformat(text, std::make_format_args(args...));
    } catch (const std::format_error&) {
        return text;
    }
}

} // namespace yqlora
