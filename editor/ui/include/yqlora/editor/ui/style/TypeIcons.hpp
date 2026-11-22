#pragma once

#include <string_view>

namespace yqlora {

class TypeInfo;
class TypeRegistry;

} // namespace yqlora

namespace yqlora::editor {

// Значки типов узлов и компонентов: дерево сцены, окно выбора типа,
// заголовки компонентов, гизмо света. Одна таблица в TypeIcons.cpp; новый
// тип - строка в ней, наследник без своей строки берёт значок базового.

// Значок из таблицы у типа или ближайшего базового; nullptr - его нет ни у
// кого в цепочке до корня (Node, Component), а значок корня у наследника не
// считается.
const char* findTypeIcon(const TypeRegistry& registry, const TypeInfo& type);

// То же, без значка - значок корня его вида (узел или компонент); скрипт без
// своей строки - значок скрипта.
const char* typeIcon(const TypeRegistry& registry, const TypeInfo& type);

// Значок по одному имени, без реестра: строка таблицы или значок узла.
const char* typeIcon(std::string_view type);

} // namespace yqlora::editor
