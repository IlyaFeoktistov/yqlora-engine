# Yqlora Engine

[Русская версия](README.ru.md)

![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20Windows-lightgrey.svg)

A 3D game engine with an editor, written in C++20.

> **Status:** early development.

## Roadmap

MVP: everything needed to build a small game prototype.

- [x] Scene tree, components, transforms, scene files
- [x] Renderer basics: meshes, editor grid, selection outline
- [x] Editor: project manager, scene tree, inspector, files, manipulators
- [x] Play / Stop in the editor
- [x] C++ scripts on nodes ([first script](docs/guide/first-script.md))
- [x] Building scripts from the editor, hot reload
- [x] Meshes from files (glTF) and textures ([importing a model](docs/guide/import-model.md))
- [x] Materials, custom shaders, lights, sky, sun and shadows ([your own shader, material and sky](docs/guide/shader-material-sky.md))
- [x] Prefabs with overrides, nesting and variants, creating objects in a game and a pool ([prefabs](docs/guide/prefabs.md))
- [ ] Physics and collisions
- [ ] Audio
- [ ] Game UI
- [ ] Running a game without the editor
- [ ] Documentation: public API, engine internals, editor user guide

After the MVP: a photorealistic picture by default
([post-processing and photorealism](docs/architecture/rendering.md#photorealism-by-default-planned-after-the-mvp)),
then Luau scripts, then 2D, then visual scripting (blocks and links
instead of code, attached to a node like any other script).

Architecture: [overview and documents by subsystem](docs/architecture/README.md).

## Building

Requirements: CMake 3.26+, a C++20 compiler (GCC 13+, MinGW-w64 or Visual Studio 2022), Ninja.
Libraries are downloaded by CMake during configuration.

### Linux / WSL

```bash
sudo apt install build-essential cmake ninja-build libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxss-dev libxfixes-dev libxtst-dev libwayland-dev \
  libxkbcommon-dev libegl-dev libdecor-0-dev

make run    # build and run
make test   # build and run the tests
```

### Windows

```powershell
cmake --preset debug          # MinGW-w64 + Ninja; with Visual Studio use the vs2022 preset
cmake --build --preset debug
.\build\debug\bin\yqlora.exe
```

The program starts without a console and logs only warnings and errors;
`yqlora.exe --debug` logs everything and shows the log in a console.

## Third-party

| Component | License |
|---|---|
| [SDL3](https://github.com/libsdl-org/SDL) | zlib |
| [glm](https://github.com/g-truc/glm) | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) | MIT |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | MIT |
| [cgltf](https://github.com/jkuhlmann/cgltf) | MIT |
| [stb_image](https://github.com/nothings/stb) | MIT or public domain |
| [MikkTSpace](https://github.com/mmikk/MikkTSpace) | zlib |
| [glslang](https://github.com/KhronosGroup/glslang) | BSD-3-Clause and others (see its LICENSE.txt) |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | Apache-2.0 |
| [doctest](https://github.com/doctest/doctest) | MIT |
| [glad](https://github.com/Dav1dde/glad) (generated loader) | CC0-1.0 or WTFPL; Khronos parts Apache-2.0 |
| [Noto Sans](https://github.com/notofonts/latin-greek-cyrillic) font | [SIL OFL 1.1](assets/fonts/OFL.txt) |
| [Lucide](https://lucide.dev) icons | [ISC](assets/fonts/Lucide-LICENSE.txt) |

## License

[MIT](LICENSE)
