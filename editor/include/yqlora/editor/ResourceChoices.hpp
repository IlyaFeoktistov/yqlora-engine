#pragma once

#include "yqlora/resource/ResourceId.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {
class ResourceCache;
class ResourceDatabase;
} // namespace yqlora

namespace yqlora::editor {

// Что можно выбрать в поле ссылки на ресурс (окно выбора).
struct ResourceChoice {
    ResourceId id = 0;
    // Имя в списке: имя встроенного ресурса, путь файла или имя вложенного.
    std::string label;
    // Файл, в котором лежит вложенный ресурс; пусто у встроенных и у файлов.
    std::string group;
    // Встроенный ресурс движка (cube, Standard), а не файл проекта.
    bool builtin = false;
};

// Вид ресурса файла path: по загрузчику, а сцена - SceneSerializer::kResourceKind
// (её загрузчик не читает). Пусто - движок такие файлы не читает.
std::string_view fileKind(const std::filesystem::path& path, const ResourceCache& cache);

// Вложенные ресурсы файла file (путь path), которые в нём сейчас есть, в
// порядке файла. Файл читается, только если в таких файлах бывают вложенные
// ресурсы (ResourceCache::nestedKindsOf).
std::vector<ResourceId> nestedResources(ResourceId file, const std::filesystem::path& path, ResourceCache& cache);

// Всё, что подходит полю ссылки на ресурсы вида kind (T::kResourceKind):
// встроенные ресурсы, затем ресурсы проекта по путям, вложенные - под своим
// файлом. Файлы с вложенными ресурсами этого вида загружаются: так видно,
// какие из них ещё есть в файле. resources - nullptr без проекта.
std::vector<ResourceChoice> resourceChoices(
    std::string_view kind,
    const ResourceDatabase* resources,
    ResourceCache& cache
);

// Что положить в поле вида kind, если на него отпустили ресурс id: сам
// ресурс, если он подходит, а файл с вложенными ресурсами этого вида -
// первый из них (модель на поле меша - её первый меш). nullopt -
// ресурс не подходит.
std::optional<ResourceId> resourceForField(
    std::string_view kind,
    ResourceId id,
    const ResourceDatabase* resources,
    ResourceCache& cache
);

} // namespace yqlora::editor
