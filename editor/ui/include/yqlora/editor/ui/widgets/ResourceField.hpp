#pragma once

#include "yqlora/editor/ui/widgets/PropertyEditor.hpp"

#include <string_view>

namespace yqlora::editor {

// Поле ссылки на ресурс вида kind (T::kResourceKind) с назначением use
// (uses::kSkyMaterial, пусто - любое): иконка и
// имя ресурса ("Нет (Меш)", если пусто), щелчок по полю или по кнопке справа
// открывает окно выбора (поиск, "Нет", встроенные ресурсы, ассеты проекта;
// вложенные - под своими файлами, у текстур - с превью). На поле можно отпустить ресурс из
// "Файлов", см. resourceForField. У текстуры под полем - её превью.
// true, если значение поменяли.
bool resourceField(
    std::string_view kind,
    std::string_view use,
    ResourceRef& value,
    const PropertyEditorContext& context
);

} // namespace yqlora::editor
