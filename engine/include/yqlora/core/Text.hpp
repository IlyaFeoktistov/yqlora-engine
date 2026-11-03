#pragma once

#include "yqlora/core/Export.hpp"

#include <string>
#include <string_view>

namespace yqlora {

// Строчные латинские буквы, остальные байты (в том числе UTF-8) как есть:
// не зависит от локали системы.
YQLORA_API std::string toLowerAscii(std::string_view text);

// part входит в text без учёта регистра латинских букв (поиск в списках
// редактора). Пустой part входит в любой text.
YQLORA_API bool containsIgnoringCase(std::string_view text, std::string_view part);

} // namespace yqlora
