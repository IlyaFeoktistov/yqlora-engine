# Yqlora Engine

[English version](README.md)

![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20Windows-lightgrey.svg)

3D игровой движок с редактором на C++20.

> **Статус:** ранняя разработка.

## Планы

MVP: всё, что нужно для небольшого прототипа игры.

- [x] Дерево сцены, компоненты, трансформы, файлы сцен
- [x] Основа рендера: меши, сетка редактора, контур выбранного объекта
- [x] Редактор: менеджер проектов, дерево сцены, инспектор, файлы, манипуляторы
- [x] Play / Stop в редакторе
- [x] Скрипты на C++ на узлах ([первый скрипт](docs/guide/first-script.ru.md))
- [x] Сборка скриптов из редактора, горячая перезагрузка
- [x] Меши из файлов (glTF) и текстуры ([импорт модели](docs/guide/import-model.ru.md))
- [ ] Материалы, свои шейдеры, свет, небо, солнце и тени
- [ ] Физика и столкновения
- [ ] Звук
- [ ] Интерфейс игрока
- [ ] Запуск игры без редактора
- [ ] Документация: публичный API, внутреннее устройство движка, руководство по редактору

После MVP: фотореалистичная картинка по умолчанию
([постобработка и фотореализм](docs/architecture/rendering.ru.md#фотореализм-по-умолчанию-planned-after-the-mvp)),
скрипты на Luau, затем 2D, затем визуальный скриптинг (блоки и связи
вместо кода, вешается на узел как любой другой скрипт).

Архитектура: [обзор и документы по подсистемам](docs/architecture/README.ru.md).

## Сборка

Нужны CMake 3.26+, компилятор C++20 (GCC 13+, MinGW-w64 или Visual Studio 2022) и Ninja.
Библиотеки CMake скачивает сам при конфигурации.

### Linux / WSL

```bash
sudo apt install build-essential cmake ninja-build libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxss-dev libxfixes-dev libxtst-dev libwayland-dev \
  libxkbcommon-dev libegl-dev libdecor-0-dev

make run    # собрать и запустить
make test   # собрать и прогнать тесты
```

### Windows

```powershell
cmake --preset debug          # MinGW-w64 + Ninja; с Visual Studio пресет vs2022
cmake --build --preset debug
.\build\debug\bin\yqlora.exe
```

Программа запускается без консоли и пишет в лог только предупреждения и ошибки;
`yqlora.exe --debug` пишет всё и показывает лог в консоли.

## Сторонние компоненты

| Компонент | Лицензия |
|---|---|
| [SDL3](https://github.com/libsdl-org/SDL) | zlib |
| [glm](https://github.com/g-truc/glm) | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) | MIT |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | MIT |
| [cgltf](https://github.com/jkuhlmann/cgltf) | MIT |
| [stb_image](https://github.com/nothings/stb) | MIT или общественное достояние |
| [glslang](https://github.com/KhronosGroup/glslang) | BSD-3-Clause и другие (см. его LICENSE.txt) |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | Apache-2.0 |
| [doctest](https://github.com/doctest/doctest) | MIT |
| [glad](https://github.com/Dav1dde/glad) (сгенерированный загрузчик) | CC0-1.0 или WTFPL; части Khronos - Apache-2.0 |
| Шрифт [Noto Sans](https://github.com/notofonts/latin-greek-cyrillic) | [SIL OFL 1.1](assets/fonts/OFL.txt) |
| Иконки [Lucide](https://lucide.dev) | [ISC](assets/fonts/Lucide-LICENSE.txt) |

## Лицензия

[MIT](LICENSE)
