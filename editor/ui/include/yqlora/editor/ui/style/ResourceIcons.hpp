#pragma once

#include <string_view>

namespace yqlora::editor {

// Значок ресурса вида kind (T::kResourceKind, kFolderKind, kCppScriptKind):
// один для Файлов, поля ресурса и меню "Создать ресурс". Новый вид -
// строка в таблице ResourceIcons.cpp; без неё - значок файла.
const char* resourceIcon(std::string_view kind);

} // namespace yqlora::editor
