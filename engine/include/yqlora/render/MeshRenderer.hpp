#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/material/MaterialOverrides.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/scene/Component.hpp"

#include <cstddef>
#include <vector>

namespace yqlora {

// Рисует меш в положении узла. Живёт только на Node3D (requiresNode): положение берётся из его Transform.
class YQLORA_API MeshRenderer final : public Component {
public:
    YQLORA_TYPE(MeshRenderer, Component)

    // Ресурс меша: встроенный (builtin::kCubeMesh и другие) или из файла
    // проекта. Пустая ссылка (по умолчанию) - меша нет, и компонент ничего
    // не рисует, как и с отсутствующим или битым мешем.
    ResourceRef mesh() const
    {
        return m_mesh;
    }

    void setMesh(ResourceRef mesh)
    {
        m_mesh = mesh;
    }

    // Материалы по подмешам меша: слот i - подмеш i. Пустой слот или слот за
    // концом списка рисуется материалом по умолчанию (builtin::kDefaultMaterial).
    const std::vector<ResourceRef>& materials() const
    {
        return m_materials;
    }

    void setMaterials(std::vector<ResourceRef> materials)
    {
        m_materials = std::move(materials);
    }

    // Материал слота; пустая ссылка, если слота нет.
    ResourceRef material(std::size_t slot) const
    {
        return slot < m_materials.size() ? m_materials[slot] : ResourceRef{};
    }

    // Замены полей материала слота только у этого объекта (см.
    // MaterialOverrides). Не сохраняются со сценой.
    MaterialOverrides& overrides(std::size_t slot);

    // nullptr, если у слота нет замен.
    const MaterialOverrides* findOverrides(std::size_t slot) const;

private:
    ResourceRef m_mesh;
    std::vector<ResourceRef> m_materials;
    std::vector<MaterialOverrides> m_overrides;
};

} // namespace yqlora
