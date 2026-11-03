#include "yqlora/core/Text.hpp"

#include <algorithm>

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
    return !std::ranges::search(text, part, {}, lowerAscii, lowerAscii).empty() || part.empty();
}

} // namespace yqlora
