#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/project/SettingsSection.hpp"
#include "yqlora/scene/Layers.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

// Слой 0 есть всегда, под этим именем, и не переименовывается.
inline constexpr std::string_view kDefaultLayerName = "Default";

// Раздел Layers настроек проекта: имена kLayerCount слоёв. Узлы и маски
// камер хранят номера, поэтому переименование
// слоя сцены не меняет. Слой без имени не используется и не попадает в списки.
class YQLORA_API LayerSettings final : public SettingsSection {
public:
    YQLORA_TYPE(LayerSettings, SettingsSection)

    LayerSettings();

    // Ровно kLayerCount имён, по номеру слоя.
    const std::vector<std::string>& names() const
    {
        return m_names;
    }

    // Лишние имена отбрасываются, недостающие пустые; у слоя 0 имя всегда kDefaultLayerName.
    void setNames(std::vector<std::string> names);

    // Пусто, если слой без имени или номера такого нет.
    std::string_view name(int layer) const;

private:
    std::vector<std::string> m_names;
};

} // namespace yqlora
