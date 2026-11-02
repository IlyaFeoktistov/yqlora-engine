# Architecture

[Русская версия](README.ru.md)

Contracts of the engine subsystems, one document per subsystem. Each changes
together with the code it describes.

Status marks: **[done]** implemented, **[next]** the next stage of work,
**[planned]** later, with the step it belongs to. **[temporary]** marks a
list of fixes rather than a description: it is removed once done, and the
documents keep only how the engine works.

| Document | What it covers |
|---|---|
| [Scene](scene.md) | nodes, components, resources, lifecycle, references and serialization |
| [Types and reflection](reflection.md) | type registry, properties, methods, types declared at run time |
| [Engine context and the frame](frame.md) | engine services, game time, scene systems, the order of a frame |
| [Rendering](rendering.md) | cameras, layers, views, the RHI, shaders, Prepare and the render graph |
| [Shaders, materials and lights](materials.md) | project shaders, materials and their slots, light nodes (step 5 plan) |
| [Scripts](scripting.md) | C++, Luau, visual scripts, hot reload |
| [Editor](editor.md) | parts of the editor: session, document, Play, panels |
| [Code editor](code-editor.md) | the Scripts tab: Luau with highlighting and hints, later shaders |
| [Plugins](plugins.md) | ready-made scripts, scenes and resources from the plugin repository, the plugin manager |
| [Project settings](project-settings.md) | settings of a game kept in its project |
| [Project and file formats](project.md) | the project folder, JSON files, format versions, upgrading a project |
| [Resources](resources.md) | resource IDs, `.meta` files, built-in resources, meshes and textures from files, the resource cache |
| [Localization](localization.md) | language files, keys, `tr()`, translation rules |
| [Input](input.md) | raw input, input actions and bindings, gamepads |

How the code is written (layout, design rules, style, commits) is in
[CONTRIBUTING](../../CONTRIBUTING.md).

## Code layout

Each subsystem is a folder of its own and registers its own types
(`registerSceneTypes`, `registerRenderTypes`, `registerProjectTypes` in `internal/`);
`registerEngineTypes` in `core/` only calls them in order. Public headers are
in `include/`, headers for the engine and the editor only in `internal/`,
`src/` holds only `.cpp` files. The same holds outside the engine: the editor
logic (`editor/include/yqlora/editor/`, `editor/src/`), the editor interface
(`editor/ui/include/yqlora/editor/ui/`, `editor/ui/src/`) and the OpenGL
backend (`backends/opengl/internal/yqlora/opengl/`, `backends/opengl/src/`,
namespace `yqlora::opengl`, so the classes of the future Vulkan and D3D12
backends do not clash by name; a backend holds only the RHI device and the
ImGui integration).
Headers are always included as `"yqlora/<part>/X.hpp"`.

```
engine/include/yqlora/core/        engine services, engine types, log, version
engine/include/yqlora/reflection/  TypeRegistry, Property, Method, component rules
engine/include/yqlora/scene/       Node, Node3D, Transform3D, Component, Scene, scene systems
engine/include/yqlora/project/     Project, ProjectSettings, SettingsSection, GeneralSettings
engine/include/yqlora/scripting/   Script, ModuleRegistrar; GameModule in internal/
engine/include/yqlora/render/      Camera3D, MeshRenderer, RenderSystem; lights        [step 5]
engine/include/yqlora/physics/     RigidBody, colliders, PhysicsWorld                  [step 6]
engine/include/yqlora/audio/       AudioSource, AudioListener, AudioWorld              [step 7]
engine/include/yqlora/ui/          Control, Label, Button                              [step 8]
examples/scripts/                  example game module (Rotator), built as game.dll / libgame.so
```

Platform and interface **[done]**: the interface layer (`UiLayer`, Dear
ImGui) lives in `ui/` and gets window events through `WindowEvents::onEvent`,
so the window knows nothing about ImGui; system dialogs are the platform
`FileDialogs`, not part of `Window`; upgrading a project to the current format
is the free function `upgradeProject` of `project/`.
