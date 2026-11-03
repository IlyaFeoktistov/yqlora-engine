#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/project/SettingsSection.hpp"
#include "yqlora/reflection/EnumNames.hpp"

#include <array>
#include <string_view>

namespace yqlora {

// Как собирать модуль игры. Меняет только оптимизацию и отладочные сведения:
// библиотека времени выполнения всегда та же, что у движка.
enum class ScriptConfiguration {
    Debug,
    Release,
};

// Раздел Scripts настроек проекта: сборка модуля игры из папки scripts/.
class YQLORA_API ScriptsSettings final : public SettingsSection {
public:
    YQLORA_TYPE(ScriptsSettings, SettingsSection)

    ScriptConfiguration configuration = ScriptConfiguration::Debug;
};

} // namespace yqlora

// Имена конфигураций - они же CMAKE_BUILD_TYPE.
YQLORA_ENUM_NAMES(yqlora::ScriptConfiguration, "Debug", "Release")
