#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace yqlora::editor {

// Комментарий // прямо над объявлением class или struct с именем className
// в тексте source: строки без //, через пробел, пустая строка комментария -
// новый абзац. Так скрипт описан для окна выбора, даже если в регистрации нет
// .description(). Пусто, если объявления или комментария над ним нет.
std::string classComment(std::string_view source, std::string_view className);

// Комментарий над классом скрипта className: в заголовке рядом с его
// исходником sourceFile (то же имя, .hpp или .h), иначе в самом исходнике.
std::string scriptComment(const std::filesystem::path& sourceFile, std::string_view className);

} // namespace yqlora::editor
