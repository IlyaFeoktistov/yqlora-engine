#pragma once

#include "yqlora/reflection/Property.hpp"

#include <string>

namespace yqlora {
class ResourceDatabase;
} // namespace yqlora

namespace yqlora::editor {

// Как ссылка на ресурс выглядит в инспекторе: имя встроенного ресурса, путь
// файла в проекте ("models/door.glb / Door" у вложенного), "нет" для пустой
// ссылки или "не найден".
// resources - ресурсы открытого проекта, nullptr без проекта.
std::string resourceLabel(ResourceRef value, const ResourceDatabase* resources);

} // namespace yqlora::editor
