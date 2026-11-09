#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/Record.hpp"
#include "yqlora/reflection/TypeName.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/TimerSystem.hpp"

#include <string_view>
#include <tuple>

namespace yqlora {

class GameTime;
class Input;
class Scene;
class Transform3D;

// Имя типа скрипта на C++ и его база Script. Ставится в открытой части класса:
//
//   class Player : public yqlora::Script {
//   public:
//       YQLORA_SCRIPT(Player)
//       ...
#define YQLORA_SCRIPT(Class) YQLORA_TYPE(Class, ::yqlora::Script)

// Поля скрипта для инспектора и файла сцены. Внутри класса, в любой его части,
// поэтому перечислить можно и закрытые поля:
//
//   YQLORA_FIELDS(
//       field("speed", &Player::speed).range(0.0f, 100.0f),
//       field("jumpHeight", &Player::m_jumpHeight)
//   )
//
// Имя в field - постоянный идентификатор в файлах; переименованное поле
// помечают .formerly("старое имя"). Атрибуты - как у любого свойства, см.
// PropertyAttributes.
#define YQLORA_FIELDS(...)                                                                                             \
    friend struct ::yqlora::ScriptFields;                                                                              \
    static auto yqloraFields()                                                                                         \
    {                                                                                                                  \
        using ::yqlora::field;                                                                                         \
        using ::yqlora::PropertyHint;                                                                                  \
        return std::tuple(__VA_ARGS__);                                                                                \
    }

// Поведение на C++: компонент с полным жизненным циклом и короткими путями к
// движку. Это база только для C++; скрипты Luau и графы - свои классы
// компонентов (см. docs/architecture/scripting.md).
class YQLORA_API Script : public Component {
public:
    YQLORA_TYPE(Script, Component)

protected:
    // Сам по себе Script не компонент: его наследуют скрипты игры.
    Script() = default;

    // Пути к движку через узел и его сцену; скрипт не владеет ни одной подсистемой.
    // Вызывать, пока скрипт в сцене (с onEnterScene / onEnable до onStop).
    Node& node() const;
    Scene& scene() const;
    // Положение узла; узел скрипта должен быть пространственным (Node3D).
    Transform3D& transform() const;
    // Ввод кадра с действиями из настроек проекта.
    const Input& input() const;
    // Время игры: delta, шкала, фиксированный шаг.
    const GameTime& time() const;
    // Первый узел сцены с таким именем; nullptr, если нет.
    Node* findNode(std::string_view name) const;
    // Таймеры от имени этого скрипта: timers().after(2.0f, [this] { ... }).
    // Снимаются, когда скрипт уходит из сцены, и ждут, пока он выключен.
    Timers timers();
};

// Доступ к закрытому списку полей скрипта (YQLORA_FIELDS) для регистрации (ModuleRegistrar).
struct ScriptFields {
    template <typename T>
    static auto of()
    {
        if constexpr (requires { T::yqloraFields(); }) {
            return T::yqloraFields();
        } else {
            return std::tuple<>();
        }
    }
};

} // namespace yqlora
