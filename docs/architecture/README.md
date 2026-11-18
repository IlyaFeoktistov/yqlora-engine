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
| [Post-processing](post-processing.md) | auto exposure, bloom, anti-aliasing, depth of field, grading, volumes, a game's own effects (after the MVP) |
| [Shader graph](shader-graph.md) | shaders from blocks compiled to `.yshader`, the graph editor shared with visual scripts (after the MVP) |
| [Shaders, materials and lights](materials.md) | shaders, materials and their slots, lights, sky and atmosphere, ambient light, shadows, glTF materials |
| [Noise](noise.md) | noise in code, scripts, textures and shaders from one set of algorithms |
| [Animation](animation.md) | the animation resource, `AnimationPlayer`, the Animation panel with keys |
| [Particles](particles.md) | particle effects: the effect resource, `ParticleEmitter`, instanced drawing |
| [Wind](wind.md) | wind zones, gusts, one wind field for vegetation, cloth and particles on the CPU and the GPU (after the MVP) |
| [Terrain and vegetation](terrain.md) | height map terrain with LOD, layers, holes, brushes; grass and placed trees and rocks (after the MVP) |
| [Game interface](ui.md) | controls, layout, text, themes, focus and input, menus and run-time settings (step 8) |
| [Audio](audio.md) | clips, sources and the listener, buses and effects, voices (step 7) |
| [Documentation and the release](documentation.md) | API reference, the site, the editor guide, the tutorial, release 0.0.2 (step 10) |
| [Building](build.md) | the player, cooking and the import cache, the package, scene changes, saves (step 9) |
| [Physics](physics.md) | rigid bodies, colliders, contacts and triggers, queries, the character controller, joints (step 6) |
| [Cameras](cameras.md) | camera rigs: flight, first and third person, following, transitions between shots (after step 6) |
| [Blockout](blockout.md) | grid materials, block shapes, drawing and pulling a test level in the scene view (after cameras) |
| [Cloth and ropes](cloth.md) | soft bodies: cloth and soft ropes with pinning, wind and a speed budget (after the MVP) |
| [Navigation and player control](navigation.md) | navigation map, path finding, agents, `PlayerController` (after the MVP) |
| [Scripts](scripting.md) | C++, Luau, visual scripts, hot reload, scene access from code, timers |
| [Prefabs](prefabs.md) | prefabs with overrides, nesting and variants, creating objects in a game |
| [Events](events.md) | events of components from code and wired in the inspector |
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

## Order of work [planned]

The rest of the MVP, in this order; each stage has its plan with phases
(a commit per phase) in the document of its subsystem.

1. **Scripts and prefabs** **[done]**: scene access from code (SA.1, SA.2), IDs
   (PF.1), references in the inspector (SA.3), events (EV.1, EV.2), timers
   (T.1, T.2), prefabs (PF.2 to PF.5), the external code editor (CE.1,
   CE.2), then the documents of the stage (SA.4, PF.6, EV.3, T.3, CE.3). See [scripting](scripting.md),
   [prefabs](prefabs.md), [events](events.md), [editor](editor.md).
2. **Physics** (step 6): [physics](physics.md).
3. **Descriptions and the type picker** (PD.1, AC.1): a tooltip with what a
   field does and its usual values, see
   [editor](editor.md#descriptions-of-properties-planned-after-step-6); a
   window of components and nodes on shelves with icons and descriptions,
   see [editor](editor.md#adding-components-and-nodes-planned-after-step-6).
4. **Cameras** (after step 6): [cameras](cameras.md).
5. **Blockout** (after cameras): [blockout](blockout.md).
6. **The game interface** (step 8): [game interface](ui.md).
7. **Running a game without the editor** (step 9): [building](build.md).
8. **Audio** (step 7): [audio](audio.md).
9. **Checking the whole MVP**: every step tried in the live editor and
   in a built game, the bugs found fixed, before the release.
10. **Documents and version 0.0.2** (step 10): [documentation and the release](documentation.md).

After the MVP: animation; particles and noise; wind, terrain and
vegetation ([wind](wind.md), [terrain](terrain.md)); cloth and ropes
([cloth](cloth.md)); post-processing, the first part of the photorealism
stage ([post-processing](post-processing.md)); Luau; the shader graph
([shader graph](shader-graph.md)); 2D; visual scripts on the graph editor
of the shader graph. The order follows the dependencies: wind needs noise,
terrain needs the instancing of particles and the wind, cloth needs the
wind and the run-time meshes of blockout, the shader graph needs the time
of wind and the depth sampling of post-processing.

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
engine/include/yqlora/render/      Camera3D, MeshRenderer, RenderSystem, lights, Environment
engine/include/yqlora/material/    MaterialOverrides, SharedMaterial (materials from scripts)
engine/include/yqlora/physics/     RigidBody, colliders, PhysicsWorld                  [step 6]
engine/include/yqlora/camera/      CameraRig, rigs, CameraDirector                     [after step 6]
engine/include/yqlora/audio/       AudioSource, AudioListener, AudioWorld              [step 7]
engine/include/yqlora/ui/          Control, Label, Button                              [step 8]
examples/scripts/                  example game module (Rotator), built as game.dll / libgame.so
```

Platform and interface **[done]**: the interface layer (`UiLayer`, Dear
ImGui) lives in `ui/` and gets window events through `WindowEvents::onEvent`,
so the window knows nothing about ImGui; system dialogs are the platform
`FileDialogs`, not part of `Window`; upgrading a project to the current format
is the free function `upgradeProject` of `project/`.
