#pragma once

#include "yqlora/editor/ui/inspectors/ResourceKindInspector.hpp"

#include "yqlora/editor/ResourceInspection.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string_view>

namespace yqlora::editor {

struct EditorContext;

// Инспектор выбранного в "Файлах" ресурса: имя, вид и ошибка загрузки, а
// дальше - инспектор его вида (ModelInspector, TextureInspector).
class ResourceInspector {
public:
    ResourceInspector();
    ~ResourceInspector();

    void draw(EditorContext& context, const ResourceInspection& inspection);

private:
    // Чей файл сейчас показан: другой файл - правка настроек начинается заново.
    ResourceId m_id = 0;
    std::map<std::string_view, std::unique_ptr<ResourceKindInspector>, std::less<>> m_inspectors;
};

} // namespace yqlora::editor
