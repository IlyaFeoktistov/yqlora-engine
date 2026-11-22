#include "yqlora/core/Text.hpp"

#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <string>

namespace yqlora {

namespace {

char lowerAscii(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

} // namespace

std::string toLowerAscii(std::string_view text)
{
    std::string result(text);
    std::ranges::transform(result, result.begin(), lowerAscii);

    return result;
}

bool containsIgnoringCase(std::string_view text, std::string_view part)
{
    // Свёртка регистра Unicode у SDL: "твёрд" находит "Твёрдое тело".
    // TODO: две копии строк на вызов ради нуля в конце. Если фильтр лога на
    // тысячах строк станет заметен - своя свёртка по string_view.
    return part.empty() || SDL_strcasestr(std::string(text).c_str(), std::string(part).c_str()) != nullptr;
}

} // namespace yqlora
