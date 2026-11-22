#pragma once

#include <string>
#include <string_view>

namespace yqlora::editor {

// Адрес страницы руководства page (docs/guide/<page>.md) в репозитории
// движка, на языке language, если у страницы есть перевод на него, иначе
// английская.
std::string guideUrl(std::string_view page, std::string_view language);

} // namespace yqlora::editor
