#pragma once

#include "yqlora/reflection/Property.hpp"

#include <string>

namespace yqlora::editor {

// Откуда подсказка свойства: явный tooltip (ключ или сам текст), иначе ключ
// описания объявившего типа help.<тип>.<имя> (Property::helpKey).
std::string propertyHelpSource(const Property& property);

// Подсказка свойства на текущем языке; пусто - описания нет (свойство
// скрипта игры может его не иметь).
std::string propertyHelp(const Property& property);

} // namespace yqlora::editor
