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
    // Явный tooltip может быть самим текстом: без перевода - он же.
    // Выведенный ключ без перевода - описания нет.
    const std::string source = propertyHelpSource(property);

    return trOr(source, source == property.helpKey() ? std::string_view() : std::string_view(source));
}

} // namespace yqlora::editor
