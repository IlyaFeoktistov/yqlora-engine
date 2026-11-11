# Первый скрипт

[English version](first-script.md)

Скрипт - это C++-класс, который вешается на узел как компонент. Скрипты -
файлы проекта, как сцены, и могут лежать в любой папке. Редактор собирает
их все в одну разделяемую библиотеку, модуль игры (`game.dll` на Windows,
`libgame.so` на Linux), тем же компилятором, что и движок, и перезагружает
её, пока вы работаете. Своих файлов сборки у проекта нет.

## 1. Создать скрипт

В "Файлах" нажмите "Создать ресурс" (или правый щелчок по папке) и выберите
"C++ скрипт". Введите имя класса, например `Spinner`. Редактор создаст в
папке `Spinner.hpp` и `Spinner.cpp` и соберёт модуль; сборка видна в панели
"Лог".

Двойной щелчок по `Spinner.cpp` открывает его в вашем редакторе кода:
выберите его в Правка > Настройки редактора (готовые настройки для Visual
Studio Code, CLion и Sublime Text или любая программа со своими
аргументами). Без него файл открывает программа, которую система назначила
для файлов C++, без строки.

## 2. Написать код

`Spinner.hpp`:

```cpp
#pragma once

#include <yqlora/scripting/Script.hpp>

class Spinner : public yqlora::Script {
public:
    YQLORA_SCRIPT(Spinner)

    float degreesPerSecond = 90.0f;

    YQLORA_FIELDS(field("degreesPerSecond", &Spinner::degreesPerSecond).range(-720.0f, 720.0f))

protected:
    void onUpdate(float deltaSeconds) override;
};
```

`Spinner.cpp`:

```cpp
#include "Spinner.hpp"

#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Transform3D.hpp>
#include <yqlora/scripting/ModuleScripts.hpp>

#include <glm/gtc/quaternion.hpp>

YQLORA_REGISTER_SCRIPT(Spinner).requiresNode<yqlora::Node3D>();

void Spinner::onUpdate(float deltaSeconds)
{
    const yqlora::Quat turn = glm::angleAxis(glm::radians(degreesPerSecond * deltaSeconds), yqlora::math::kWorldUp);
    transform().setRotation(transform().rotation() * turn);
}
```

- `YQLORA_SCRIPT(Spinner)` задаёт имя типа; это имя хранят файлы сцен.
- `YQLORA_FIELDS` перечисляет поля, которые показывает инспектор и
  сохраняют файлы сцен, с их атрибутами (`.range`, `.tooltip`, ...).
  Остальные поля живут только во время игры.
- `YQLORA_REGISTER_SCRIPT` добавляет скрипт в модуль игры, с правилами после
  него: `requiresNode`, `requiresComponent`, `formerly`, `executionOrder`,
  и его методами и событиями (`method`, `event`, см. ниже). Никакой другой
  файл скрипты не перечисляет.
- Вызовы жизненного цикла (`onStart`, `onUpdate`, ...) и их порядок описаны
  в [скриптах](../architecture/scripting.ru.md#контракт-жизненного-цикла).

Сохраните и вернитесь в редактор: он соберёт модуль и перезагрузит его.
Ctrl+B (Скрипты > Собрать) собирает в любой момент, Play тоже сначала
собирает.

## 3. Использовать

Выберите куб, нажмите "Добавить компонент": `Spinner` в группе "Скрипты".
Поменяйте `degreesPerSecond` в инспекторе, нажмите Play.

Поменяйте код ещё раз и вернитесь: сцена сохранит значения полей скриптов;
история правок очищается, потому что её команды держат объекты старого
кода. Ошибки компилятора в панели "Лог" красные (щелчок по ошибке
открывает её строку в вашем редакторе кода), главное меню отмечает
неудачную сборку, а старый модуль работает до следующей удачной сборки.

## 4. Достать другие объекты

Выключатель, который зажигает лампу на несколько секунд. Лампа задаётся в
инспекторе, а не ищется по имени; выключатель сообщает другим через событие
и сам гасит лампу таймером.

`Switch.hpp`:

```cpp
#pragma once

#include <yqlora/render/Light3D.hpp>
#include <yqlora/scene/Event.hpp>
#include <yqlora/scripting/Script.hpp>

class Switch : public yqlora::Script {
public:
    YQLORA_SCRIPT(Switch)

    yqlora::NodeRef lamp;           // drop a light from the scene tree here
    float onSeconds = 3.0f;
    yqlora::Event<bool> onToggled;  // others listen in code or in the inspector

    YQLORA_FIELDS(
        field("lamp", &Switch::lamp).node<yqlora::Light3D>(),
        field("onSeconds", &Switch::onSeconds).range(0.1f, 60.0f)
    )

    void toggle();

private:
    yqlora::TimerHandle m_off;
};
```

`Switch.cpp`:

```cpp
#include "Switch.hpp"

#include <yqlora/scene/Scene.hpp>
#include <yqlora/scripting/ModuleScripts.hpp>

YQLORA_REGISTER_SCRIPT(Switch)
    .method("toggle", &Switch::toggle)
    .event("onToggled", &Switch::onToggled, {"lit"});

void Switch::toggle()
{
    yqlora::Light3D* light = scene().find<yqlora::Light3D>(lamp);
    if (!light) {
        return;
    }
    const bool lit = !light->active();
    light->setActive(lit);
    onToggled.emit(lit);

    timers().cancel(m_off);
    if (lit) {
        m_off = timers().after(onSeconds, [this] {
            toggle();
        });
    }
}
```

- **Ссылки в инспекторе**: перетащите лампу из дерева сцены на поле
  `lamp`. `.node<Light3D>()` пускает в поле только свет; то, что не
  подходит, не принимается уже при перетаскивании. Щелчок по заполненному
  полю показывает цель в дереве. Другие поля-ссылки устроены так же:
  `ComponentRef<MeshRenderer>` берёт узел с рендерером (или компонент,
  перетащенный за заголовок в инспекторе), `Ref<MaterialData>` и
  `Ref<Prefab>` берут файлы из "Файлов".
- **Методы и события**: `.method` и `.event` после
  `YQLORA_REGISTER_SCRIPT` сообщают редактору о `toggle` и `onToggled`. У
  скрипта другого узла или у `Timer` поле события - список подписчиков:
  бросьте на него узел выключателя и выберите `toggle`, и это событие
  будет переключать лампу без кода. Подписчики работают только в Play и в
  игре.
- **Таймеры**: `timers().after` и `every` принадлежат скрипту: они
  снимаются, когда он уходит из сцены, и ждут, пока он выключен. Компонент
  `Timer` делает то же из инспектора.
- **Поиск без ссылки**: `node().find("Arm/Hand")` (путь от этого узла,
  `..` - родитель), `findNode("Door")` (первый узел с именем),
  `scene().group("enemies")` (узлы группы, заданной в инспекторе),
  `getComponent<T>()`, `findComponentInChildren<T>()`,
  `findComponentInParent<T>()`. Подробнее - в
  [скриптах](../architecture/scripting.ru.md#доступ-к-сцене-из-кода-done) и
  [событиях](../architecture/events.ru.md).

## Ограничения

- Скрипты - нативный код в процессе редактора. Исключение из вызова
  жизненного цикла выключает компонент и уходит в лог, но падение (нулевой
  указатель) роняет редактор вместе со скриптом, а бесконечный цикл его
  вешает. Сохраняйтесь перед Play.
- Один модуль игры на проект. Скрипт с уже занятым именем (типом движка или
  другим скриптом) не регистрируется; в логе написано, каким модулем занято
  имя.
- Имя поля в `field("name", ...)` хранится в файлах сцен. Переименовав его,
  отметьте старое: `.formerly("oldName")`; переименовав класс -
  `YQLORA_REGISTER_SCRIPT(T).formerly("OldName")`.
- Если модуль не собран, скрипты в сценах хранятся заглушками: инспектор
  показывает их как не найденные, сохранение пишет их данные обратно без
  изменений, и они возвращаются со следующей удачной сборкой.
- Файлы в скрытых папках (имя с точки) не собираются.
- Редактор собирает тем компилятором, которым собран движок
  (`sdk/toolchain.json` рядом с редактором). Движок, собранный MSVC, пока не
  собирает скрипты из редактора.
