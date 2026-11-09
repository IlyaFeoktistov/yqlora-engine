# Архитектура

[English version](README.md)

Контракты подсистем движка, по документу на подсистему. Каждый меняется
вместе с кодом, который описывает.

Пометки статуса: **[done]** сделано, **[next]** следующий этап работы,
**[planned]** позже, с шагом, к которому относится. **[temporary]** -
список исправлений, а не описание: когда он сделан, его удаляют, и в
документах остаётся только то, как устроен движок.

| Документ | О чём |
|---|---|
| [Сцена](scene.ru.md) | узлы, компоненты, ресурсы, жизненный цикл, ссылки и сериализация |
| [Типы и рефлексия](reflection.ru.md) | реестр типов, свойства, методы, типы, объявленные во время выполнения |
| [Контекст движка и кадр](frame.ru.md) | службы движка, игровое время, системы сцены, порядок кадра |
| [Рендер](rendering.ru.md) | камеры, слои, виды, RHI, шейдеры, Prepare и граф проходов |
| [Шейдеры, материалы и свет](materials.ru.md) | шейдеры, материалы и их слоты, свет, небо и атмосфера, окружающий свет, тени, материалы glTF |
| [Шумы](noise.ru.md) | шумы в коде, скриптах, текстурах и шейдерах из одного набора алгоритмов |
| [Анимация](animation.ru.md) | ресурс анимации, `AnimationPlayer`, панель "Анимация" с ключами |
| [Частицы](particles.ru.md) | эффекты частиц: ресурс эффекта, `ParticleEmitter`, инстансная отрисовка |
| [Физика](physics.ru.md) | твёрдые тела, коллайдеры, запросы, скрепления (шаг 6) |
| [Навигация и управление игроком](navigation.ru.md) | карта навигации, поиск путей, агенты, `PlayerController` (после MVP) |
| [Скрипты](scripting.ru.md) | C++, Luau, визуальные скрипты, горячая перезагрузка |
| [Редактор](editor.ru.md) | части редактора: сессия, документ, Play, панели |
| [Редактор кода](code-editor.ru.md) | вкладка "Скрипты": Luau с подсветкой и подсказками, позже шейдеры |
| [Плагины](plugins.ru.md) | готовые скрипты, сцены и ресурсы из репозитория плагинов, менеджер плагинов |
| [Настройки проекта](project-settings.ru.md) | настройки игры, которые лежат в её проекте |
| [Проект и форматы файлов](project.ru.md) | папка проекта, файлы JSON, версии форматов, перевод проекта |
| [Ресурсы](resources.ru.md) | ID ресурсов, файлы `.meta`, встроенные ресурсы, меши и текстуры из файлов, кэш ресурсов |
| [Локализация](localization.ru.md) | файлы языков, ключи, `tr()`, правила перевода |
| [Ввод](input.ru.md) | сырой ввод, действия ввода и привязки, геймпады |

Как пишется код (раскладка, правила устройства, стиль, коммиты) - в
[CONTRIBUTING](../../CONTRIBUTING.ru.md).

## Раскладка кода

Каждая подсистема - своя папка и сама регистрирует свои типы
(`registerSceneTypes`, `registerRenderTypes`, `registerProjectTypes` в `internal/`);
`registerEngineTypes` в `core/` только вызывает их по порядку. Публичные
заголовки лежат в `include/`, заголовки только для движка и редактора - в
`internal/`, в `src/` только файлы `.cpp`. Так же и вне движка: логика
редактора (`editor/include/yqlora/editor/`, `editor/src/`), интерфейс
редактора (`editor/ui/include/yqlora/editor/ui/`, `editor/ui/src/`) и бэкенд
OpenGL (`backends/opengl/internal/yqlora/opengl/`, `backends/opengl/src/`,
пространство имён `yqlora::opengl`, чтобы классы будущих бэкендов Vulkan и
D3D12 не совпали по именам; в бэкенде только устройство RHI и интеграция
ImGui).
Заголовки подключаются всегда как `"yqlora/<часть>/X.hpp"`.

```
engine/include/yqlora/core/        engine services, engine types, log, version
engine/include/yqlora/reflection/  TypeRegistry, Property, Method, component rules
engine/include/yqlora/scene/       Node, Node3D, Transform3D, Component, Scene, scene systems
engine/include/yqlora/project/     Project, ProjectSettings, SettingsSection, GeneralSettings
engine/include/yqlora/scripting/   Script, ModuleRegistrar; GameModule in internal/
engine/include/yqlora/render/      Camera3D, MeshRenderer, RenderSystem, свет, Environment
engine/include/yqlora/material/    MaterialOverrides, SharedMaterial (материалы из скриптов)
engine/include/yqlora/physics/     RigidBody, colliders, PhysicsWorld                  [step 6]
engine/include/yqlora/audio/       AudioSource, AudioListener, AudioWorld              [step 7]
engine/include/yqlora/ui/          Control, Label, Button                              [step 8]
examples/scripts/                  example game module (Rotator), built as game.dll / libgame.so
```

Платформа и интерфейс **[done]**: слой интерфейса (`UiLayer`, Dear ImGui)
живёт в `ui/` и получает события окна через `WindowEvents::onEvent`, так что
окно ничего не знает про ImGui; системные диалоги - платформенный
`FileDialogs`, а не часть `Window`; перевод проекта на текущий формат -
свободная функция `upgradeProject` из `project/`.
