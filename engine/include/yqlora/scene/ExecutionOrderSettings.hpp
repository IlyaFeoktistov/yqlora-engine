#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/project/SettingsSection.hpp"
#include "yqlora/reflection/Record.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

// Порядок выполнения одного типа компонента, переопределяет значение из кода.
struct ExecutionOrderEntry {
    std::string type;
    int order = 0;
};

// Раздел Execution Order настроек проекта: порядок типов компонентов внутри
// фаз кадра (меньше - раньше).
// Типа нет в списке - действует порядок из кода (BasicTypeBuilder::executionOrder).
class YQLORA_API ExecutionOrderSettings final : public SettingsSection {
public:
    YQLORA_TYPE(ExecutionOrderSettings, SettingsSection)

    // Строки с именами незагруженных типов хранятся как есть: модуль может
    // загрузиться позже.
    std::vector<ExecutionOrderEntry> entries;

    // Порядок типа из списка; при повторах - первая строка. nullopt, если типа нет.
    std::optional<int> orderOf(std::string_view type) const;
};

} // namespace yqlora

YQLORA_RECORD(
    yqlora::ExecutionOrderEntry,
    field("type", &yqlora::ExecutionOrderEntry::type, PropertyHint::ComponentType),
    field("order", &yqlora::ExecutionOrderEntry::order)
)
