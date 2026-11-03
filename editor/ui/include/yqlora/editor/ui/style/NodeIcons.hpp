#pragma once

#include <string_view>

namespace yqlora::editor {

// Значок типа узла по его имени: дерево сцены, "Добавить узел", гизмо света.
// Новый тип - строка в таблице NodeIcons.cpp; без неё - значок простого Node.
const char* nodeIcon(std::string_view type);

} // namespace yqlora::editor
