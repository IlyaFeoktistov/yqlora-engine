#pragma once

#include "yqlora/reflection/Property.hpp"

#include <utility>
#include <vector>

namespace yqlora {
class Component;
class TypeRegistry;
} // namespace yqlora

namespace yqlora::editor {

// Свойство и значение, которое ему поставить.
struct PropertyEdit {
    const Property* property = nullptr;
    PropertyValue value;
};

// Значения всех свойств компонента на миг снимка. Правку, которую ведут
// прямо на компоненте (перетаскивание ручки, подгонка нового коллайдера),
// потом отдают истории одним шагом: изменения - командами, а компонент
// возвращают к снимку, чтобы команды сделали их сами.
class ComponentSnapshot {
public:
    ComponentSnapshot(const TypeRegistry& registry, const Component& component);

    // Свойства, которые у component сейчас не такие, как в снимке, с их
    // нынешними значениями, в порядке свойств типа.
    std::vector<PropertyEdit> changes(const Component& component) const;

    void restore(Component& component) const;

private:
    std::vector<std::pair<const Property*, PropertyValue>> m_values;
};

} // namespace yqlora::editor
