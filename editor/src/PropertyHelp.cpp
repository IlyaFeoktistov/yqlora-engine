#include "yqlora/editor/PropertyHelp.hpp"

#include "yqlora/i18n/Translate.hpp"

namespace yqlora::editor {

std::string propertyHelpSource(const Property& property)
{
    const std::string& tooltip = property.attributes().tooltip;

    return tooltip.empty() ? property.helpKey() : tooltip;
}

std::string propertyHelp(const Property& property)
{
    // Явный tooltip может быть самим текстом: tr без перевода вернёт его же.
    // Выведенный ключ без перевода - описания нет.
    std::string text = tr(propertyHelpSource(property));

    return text == property.helpKey() ? std::string() : text;
}

} // namespace yqlora::editor
