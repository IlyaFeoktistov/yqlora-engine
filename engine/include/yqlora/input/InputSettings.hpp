#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/project/SettingsSection.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/reflection/Record.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace yqlora {

class EngineServices;

enum class InputActionKind : std::uint8_t {
    // Нажата, зажата, отпущена.
    Button,
    // Значение от -1 до 1: сумма привязок со своими множителями.
    Axis,
};

// Привязка действия к органу управления по имени (см. InputControl). У оси
// значение органа умножается на scale: две клавиши с 1 и -1 - ось, у мыши
// scale - чувствительность.
struct InputBinding {
    std::string control;
    float scale = 1.0f;
};

// Действие: скрипты спрашивают его по имени, а не клавишу, поэтому
// переназначение клавиш не трогает код игры.
struct InputAction {
    std::string name;
    InputActionKind kind = InputActionKind::Button;
    std::vector<InputBinding> bindings;
};

// Раздел Input настроек проекта: действия ввода и их привязки. Новый
// проект начинает со стандартного набора (defaultInputActions); к нему же
// возвращает "Сбросить к стандартному".
class YQLORA_API InputSettings final : public SettingsSection {
public:
    YQLORA_TYPE(InputSettings, SettingsSection)

    InputSettings();

    std::vector<InputAction> actions;
};

// Действия, которые нужны почти любой игре: движение, обзор, прыжок,
// ускорение, присесть, стрельба, прицел, действие, пауза. Каждое привязано
// и к клавиатуре с мышью, и к геймпаду.
YQLORA_API std::vector<InputAction> defaultInputActions();

// Раздел Input открытого проекта, а без проекта - стандартный набор действий.
YQLORA_API const InputSettings& inputSettingsOf(const EngineServices& services);

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::InputActionKind, "Button", "Axis")
YQLORA_RECORD(
    yqlora::InputBinding,
    field("control", &yqlora::InputBinding::control, PropertyHint::InputControlName),
    field("scale", &yqlora::InputBinding::scale)
)

YQLORA_RECORD(
    yqlora::InputAction,
    field("name", &yqlora::InputAction::name),
    field("kind", &yqlora::InputAction::kind),
    field("bindings", &yqlora::InputAction::bindings)
)
