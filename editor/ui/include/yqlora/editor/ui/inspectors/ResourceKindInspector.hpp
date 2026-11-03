#pragma once

#include "yqlora/resource/ResourceId.hpp"

namespace yqlora::editor {

struct EditorContext;

// Инспектор файлов одного вида: что в файле и его настройки импорта. Общую
// часть (имя, вид, ошибку загрузки) рисует ResourceInspector. Новый вид -
// наследник и строка в конструкторе ResourceInspector.
class ResourceKindInspector {
public:
    virtual ~ResourceKindInspector() = default;

    // Выбран другой файл: неприменённая правка настроек забывается.
    virtual void reset() = 0;

    virtual void draw(EditorContext& context, ResourceId id) = 0;
};

// Правка настроек импорта копится в инспекторе и попадает в .meta по кнопке
// "Применить".
enum class ImportAction {
    None,
    Apply,
    Revert,
};

// Кнопки "Применить" и "Отменить"; changed - правка отличается от .meta.
ImportAction importButtons(bool changed);

} // namespace yqlora::editor
