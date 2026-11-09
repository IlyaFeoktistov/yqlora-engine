# Scripts

[Русская версия](scripting.ru.md)

A script is a component (see [scene](scene.md)): it lives on a node and
is its own component type in every language. This document covers the
languages and hot reload.

Status marks: see [README](README.md).

## Languages

**Scripting languages:** C++ first (MVP), then Luau after the MVP, then
   visual scripts after 2D.

## Boundaries: one behaviour system, several languages

Behaviour is written in C++ now, in Luau and visual graphs later. They share
everything except how the code runs:

```
Editor, inspector, serializer, graph palette, Luau bindings
        |   read only the type registry
Type registry: types, properties (with attributes), methods, phases,
               module, "is a script"
        |   filled by
C++ classes (ClassTypeBuilder)   Luau scripts, visual graphs (TypeBuilder)
        |   instances are
Component: the lifecycle contract (onEnable ... onStop), phase lists,
           enabled, owner node
        |   implemented by
user C++ class : Script          LuauScript (VM)      VisualScript (graph)
```

- **`Component` is the contract.** The lifecycle, the phase lists, enabling
  and exception handling belong to every component; a language backend only
  decides what a call does.
- **The type registry is the one source of metadata.** A property is
  described by values and accessor functions, so a C++ field, a Luau
  variable and a graph input are the same kind of property. The inspector,
  the serializer and the property menu never know which language a field
  comes from.
- **Field attributes live in `Property`** (range, display name, tooltip,
  group, not saved, not shown), so every language gets them at once.
- **"Is a script"** is a mark of the type in the registry, set by every
  backend; "Add component" groups scripts by it. It is not a base class.
- **`Script` is the base of C++ scripts only**: a convenience over the
  engine context the component already has (`node()`, `transform()`,
  `input()`, `time()`, `scene()`). Luau and visual scripts do not derive
  from it; they expose the same things through their bindings, generated
  from the registry.
- **No `ScriptInstance` layer.** Luau and graphs are one C++ component class
  each (`LuauScript`, `VisualScript`) bound to their resource, with their
  type registered at run time; nothing goes through a C++ adapter.
- **Modules own their types**: a C++ game library, `luau`, `visual`. A type
  name taken by another module is refused; unloading a module destroys its
  instances first.
- **Stable identifiers are names, renames are explicit.** Files hold the
  type name (`"Player"`) and property names (`"speed"`), never addresses or
  anything from a library; names keep diffs readable, and a C++ class has no
  file of its own to hold a GUID. A rename is declared where it happens:
  `field("speed", ...).formerly("moveSpeed")`,
  `registerScript<Player>().formerly("Hero")`: the old name is read, the new
  one written, no value is lost. Luau scripts are files, so their types can
  later follow the resource ID of the script.
- **No silent data loss.** A component whose type is missing keeps its JSON
  object exactly as read and writes it back unchanged. Properties a known
  type no longer has are kept with the object and written back too, so
  removing a field and bringing it back loses nothing.

## Scripts in C++ [done]

```cpp
class Player : public yqlora::Script {
public:
    YQLORA_SCRIPT(Player)

    float speed = 5.0f;

    // Inside the class, so private fields can be listed too.
    YQLORA_FIELDS(
        field("speed", &Player::speed).range(0.0f, 100.0f).tooltip("player.speed.tooltip"),
        field("jumpHeight", &Player::m_jumpHeight)
    )

protected:
    void onUpdate(float deltaSeconds) override
    {
        const Vec3 move(input().actionValue("MoveX"), 0.0f, -input().actionValue("MoveY"));
        transform().translate(move * speed * deltaSeconds);
    }

private:
    float m_jumpHeight = 1.5f;
};

// In Player.cpp: the script registers itself in the game module.
YQLORA_REGISTER_SCRIPT(Player).requiresNode<Node3D>();
```

### Decisions

- **A script is a component**, not a parallel object system: same node,
  lifecycle, phase lists, serialization and inspector as any component.
- **Fields are listed explicitly**, not taken from the access modifier: a
  field is saved and shown because it is listed, public or private. The name
  in `field("speed", ...)` is the stable identifier in files; the label in
  the inspector is separate (`property.<name>` or `displayName`). A field
  description can grow without breaking scripts: range, display name,
  tooltip, group, and flags for "not saved" and "not shown" (the field list
  is metadata; saving and the inspector are two users of it).
- **Field types** are the property types of the registry: bool, int, float,
  string, vectors, colour, enums, `NodeRef`, `ResourceRef`, lists and
  records. Node and resource references are stable IDs, never pointers.
- **Registration per module**: one entry function of the library
  (`YQLORA_MODULE_ENTRY`) registers its scripts through the
  `ModuleRegistrar` it is given, which records them under the module. The
  game module gets that function from the SDK (`cmake/GameModuleEntry.cpp`);
  each script marks itself with `YQLORA_REGISTER_SCRIPT` in its `.cpp`,
  which records it in a list local to the library (a function-local static,
  so the order of static initialization does not matter), and the entry
  function registers the list sorted by name. Nothing reaches the engine
  before the entry function, and no file lists the scripts by hand. A type name already taken (by another module, by the same
  module twice, or as a former name) is not registered: it goes to
  `rejected()` with the module that holds it, so the loader reports it
  instead of crashing the editor. After `registerScript<T>()` only type rules
  chain: `formerly`, `requiresNode`, `requiresComponent`, `executionOrder`;
  fields come from `YQLORA_FIELDS`.
- **The script API** (`node()`, `transform()`, `input()`, `time()`,
  `scene()`, finding nodes, the log) is a convenience over the engine
  context the component already has through its scene; `Script` owns no
  subsystem.

### Lifecycle contract

The component lifecycle of the [scene](scene.md), as scripts see it:

| Call | When |
|---|---|
| `onEnable` | the script starts taking part: enabled and its node active in hierarchy; before `onStart` |
| `onStart` | once per run, before the first phase call; re-enabling does not repeat it |
| `onFixedUpdate(step)` | every fixed step of game time (physics) |
| `onUpdate(delta)` | every frame |
| `onLateUpdate(delta)` | after all `onUpdate` of the frame |
| `onDisable` | at once when the script or its node is switched off, before `onStop`; never twice: removing an already disabled script does not repeat it |
| `onStop` | the game stops or the script is removed or destroyed; only after `onStart` |

- A script disabled before its first start gets no `onStart` until it is
  enabled.
- A script that disables itself in `onUpdate` gets `onDisable` at once and
  is skipped for the rest of the phase; it is dropped from the list after
  the phase.
- Order inside a phase: the execution order of the type, then the order of
  entering (see [scene](scene.md) and Execution Order in the project
  settings).
- An exception from a call disables the script and goes to the log.

### The game module

- The scripts of a game are one shared library, the game module: one per
  project, registered under the type module `game`
  (`GameModule::moduleName`). The editor builds it from every `.cpp` of the
  project outside hidden folders, with `sdk/game-project/CMakeLists.txt`,
  so a project has no build files of its own; it loads a copy from
  `.build/reload/` (see Build and hot reload below and the
  [first script guide](../guide/first-script.md)).
- `GameModule::load` opens the library, finds `YQLORA_MODULE_ENTRY` by
  `kModuleEntrySymbol` and calls it with a `ModuleRegistrar`. A missing
  library or entry function is an error in the log, not a crash; a
  rejected type name is logged and the rest of the module works.
- `GameModule::unload` destroys every component of the module in the given
  scenes, then removes its types, then unloads the library. In the editor
  `EditorSession` owns the order: the module loads before the startup scene
  and unloads after the scene and the edit history are gone.
- Without the library a project still opens: its scripts wait as
  placeholders (`MissingComponent`) and come back with the library.

### Limits

- **Native code is trusted.** Scripts run in the editor process. An
  exception from a lifecycle call disables the component and goes to the
  log; a crash or an endless loop takes the editor down. Luau (after the
  MVP) is the sandboxed language.
- **One game module per project**, built for the same compiler and engine
  version as the editor.
- **Luau and graphs fit the same boundaries**: their types go to the same
  registry under their own modules (`luau`, `visual`), are marked as
  scripts, and their instances are components of one C++ class each
  (`LuauScript`, `VisualScript`). Nothing above the registry changes; they
  do not derive from `Script`.

### Phases of step 2

1. Field attributes in `Property` (with `formerly`) and the "is a script"
   mark in the registry (for every language); `Script`, its API,
   `YQLORA_SCRIPT`, `YQLORA_FIELDS`, `registerScript` **[done]**.
2. Module registration: the entry function, `registerScript` (with
   `formerly`), refusal of a type name taken by another module **[done]**.
3. `GameModule`: load a shared library, register its types, unload. Before
   unloading, every instance of its types is destroyed (scenes of the editor
   and of Play), then `removeModule`, then the library goes. A test module
   built by the repository goes through the whole cycle on Linux and
   Windows: load, register, create, lifecycle calls, destroy, unload
   **[done]**. The library is found by `kModuleEntrySymbol`, the name
   `YQLORA_MODULE_ENTRY` defines. `GameModule::unload` destroys the
   components in the scenes it is given; instances outside scenes (the edit
   history) are the owner's to destroy first. Types are looked up by name,
   never by `typeid`: `type_info` comparison across libraries is unreliable
   on MinGW, so `TypeRegistry::find<T>()` and `Scene::system<T>()` use the
   `YQLORA_TYPE` name.
4. Editor, in three parts:
   - 4a. A component whose type is missing (module not built, script
     deleted) is kept as a placeholder with its JSON object as read, shown
     as missing and written back unchanged, and comes back when the type
     returns (`SceneSerializer::reviveMissing`); properties a known type
     no longer has are kept and written back **[done]**.
   - 4b. The editor loads the game module of the project from
     `.build/game.dll` (`libgame.so`, `gameLibraryPath`) when the project
     opens, before its startup scene, and unloads it when it closes, after
     the scene and the edit history are gone (`EditorSession` owns the
     order); scripts in "Add component" as their own group (by the mark);
     fields in the inspector with their attributes, saving and loading of
     field values **[done]**.
   - 4c. An example game module in `examples/`, built with the repository: a
     `Rotator` script with fields (speed, axis) **[done]**. A movement
     example is physics-based and comes with step 6.
5. Documents: this one, the game module, a "first script" guide and the
   limits (native code can crash the editor; one game module per project;
   how Luau and graphs fit the same boundaries) **[done]**.

Step 3 adds the build from the editor and hot reload (see below). The
`YQLORA_FIELD()` generator built on Clang LibTooling (a full C++ parser,
never regular expressions; it writes code the compiler checks) comes after
the MVP: it needs LLVM as a dependency, and `YQLORA_FIELDS` already covers
every attribute.

## What scripts get next [planned]

What a script cannot do yet, and where each item is done. Every item is a
registered method or type, so Luau and visual scripts get it through the
bindings without extra work.

- **Timers and delayed actions**: see "Timers" below.
- **Events** (a timer ran out, a button was pressed), from code and wired
  in the inspector: see [events](events.md).
- **Creating objects in a game**: `scene().instantiate(prefab, parent)`
  for a prefab or a model, see [prefabs](prefabs.md).
- **Rays and overlaps** (step 6): `physics().raycast`, `sphereCast`,
  `overlap` in the physics world, with layers. Picking of the editor stays
  in the editor.
- **Saving game data** (step 9): `SaveData`, a record or a JSON object
  written atomically into the folder of user data of the platform, by slot
  name; the editor and Play use a folder of their own inside the project
  cache so tests of a game never touch saves of a released one.
- **Audio and the game interface**: steps 7 and 8.
- **Luau**: after the MVP (below).

## Scene access from code [planned]

Everything a person changes in the inspector a script changes from code,
with the same effect: the registry builds properties from the getters and
setters of the C++ types, so the inspector and code call one function. The
API is the established one for a scene tree with components: lookup by
path, ID and group, components by type in the node, its children and its
parents, changes deferred during a frame.

- **References set in the inspector, no lookup in code**: a field of a
  script is typed, and the inspector fills it by a drop:

  ```cpp
  class Door : public Script {
      ComponentRef<Light3D> lamp;      // drop a node with a light, or the light itself
      NodeRef hinge;                   // any node; .node<Node3D>() narrows it
      Ref<MaterialData> openMaterial;  // drop a material from Files
      Ref<Prefab> debris;              // a prefab or a model to instantiate
      YQLORA_FIELDS(field("lamp", &Door::lamp), field("hinge", &Door::hinge),
                    field("openMaterial", &Door::openMaterial), field("debris", &Door::debris))
  };
  // lamp->setIntensity(0.0f); scene().instantiate(debris, node());
  ```

  `ComponentRef<T>` holds the node and the component (`ComponentId`) and
  gives `T*`, null when it is gone; `Ref<T>` takes its resource kind from
  `T::kResourceKind`, so the type of the field is the only place it is
  written. A reference to a node inside a prefab instance survives changes
  of the prefab (see [prefabs](prefabs.md)).
- **Drops onto fields**: a node dragged from the scene tree onto a node or
  component field (a node with several components of the type opens a
  short list to pick one), a component dragged by its header in the
  inspector onto a component field, a file from Files onto a resource
  field (as now for meshes, materials and textures), several files onto a
  list field (each one becomes an element). A field that does not take
  the dragged thing shows it while dragging and refuses the drop. Choosing
  from a list with search stays for when dragging is not handy; a click on
  a filled field shows its target (the node in the tree, the file in
  Files).
- **Finding nodes**: `scene().find(NodeRef)`; `node().find("Arm/Hand")`, a
  path relative to the node (`..` for the parent), and `scene().findNode`
  for the first node with a name; `children()`, `parent()`.
- **Groups**: a node has a list of group names (`groups`, saved, set in
  the inspector); `scene().group("enemies")` gives the nodes of a group,
  kept in an index like components, so finding all enemies does not walk
  the tree.
- **Components**: `getComponent<T>()` (the first of the type or derived),
  `getComponents<T>()` (all of them, for types with `allowMultiple`),
  `findComponentInChildren<T>()`, `findComponentInParent<T>()`, and by the
  registry name (`getComponent("Rotator")`) for types declared at run time;
  `scene().components<T>()` for a whole scene.
- **Changing the tree from code**: `addChild`, `addComponent`,
  `instantiate`; `scene().destroy(node)` and `scene().destroy(component)`,
  both applied at the end of the frame; a new node or component takes part
  from the next frame.
- **Systems a script may change**: the active `Environment` and the main
  camera are returned for writing (`RenderSystem::environment()`,
  `mainCamera()`), so a day and night cycle changes the sky and the sun
  from a script.
- **By name, for Luau and visual scripts**: `get` and `set` of any
  registered property and `call` of a method on a node or component, the
  same entries the inspector uses.
- **Each step adds its own**: physics, audio and the game interface come
  with their script API in their plans (bodies and forces, queries, sources
  and buses, controls and their events).

### Phases (a commit per phase)

- **SA.1 Lookup**: `find(NodeRef)`, paths, groups with their index,
  `getComponents`, in children and parents, by registry name. Tests: a path
  with `..`; a group after a node is renamed, moved and destroyed; the
  component searches on a three-level branch.
- **SA.2 Changes from code**: `destroy(component)` deferred; writable
  `environment()` and `mainCamera()`; `get`, `set`, `call` by name. Tests:
  a component destroyed in `onUpdate` gets `onDisable` and `onStop` at the
  end of the frame, the others still run that frame; a test walks every
  registered property of every engine node and component, sets a value
  through the registry on a live scene and reads it back; light intensity,
  camera field of view and the sky material set from a script reach
  `FrameData`.
- **SA.3 References in the inspector** (after PF.1, which gives
  components their IDs): `ComponentRef<T>`, `Ref<T>`, narrowing of
  `NodeRef`; the node and component fields with drops from the tree and
  from component headers, the picker with search, showing the target;
  drops of several files onto a list. Tests (logic without the interface):
  a drop of a node onto a `ComponentRef<Light3D>` takes its light and is
  refused for a node without one; a node with two lights asks which; a
  reference survives save, load, the Play copy and a hot reload; a removed
  component makes the reference null, not dangling; three textures dropped
  onto a list make three elements in one undo step.
- **SA.4 Documents**: this section, scene (groups, deferred removal of a
  component), the guide "First script" (references in the inspector,
  finding nodes and components).

## Timers [planned]

The design is the established one of a timer manager: handles, owners,
game time.

- **`TimerSystem`**, a scene system, keeps the timers of a scene and runs
  them in `systems.update`, after `onUpdate` of components, on game time:
  pause and time scale apply. A timer may run on real time instead
  (`unscaled`, for the game interface during a pause).
- **From a script**: `timers().after(seconds, callback)` for one call and
  `timers().every(seconds, callback)` for repeated ones return a
  `TimerHandle`; `cancel`, `pause`, `resume`, `remaining` take it. The
  owner is the calling component: its timers stop when it leaves the scene
  and wait while it is disabled, so a callback of a destroyed component is
  never called.
- **Order and catching up**: timers due in one frame fire by due time, then
  by creation order. A repeated timer whose interval is shorter than the
  frame fires once per elapsed interval, so ten shots per second stay ten
  at a low frame rate. A timer made inside a callback fires at the next
  frame at the earliest. An exception in a callback is caught and logged
  like the calls of the lifecycle.
- **The `Timer` component** for timers set in the inspector: `duration`,
  `repeat`, `autostart`, `unscaled`, the event `onTimeout` (see
  [events](events.md)), methods `start`, `stop`, `remaining`. It is a
  `TimerSystem` timer owned by the component.
- **Not saved**: running timers are run-time state; a hot reload or a load
  of a scene starts from the properties (`autostart`).

### Phases (a commit per phase)

- **T.1 Timers from code**: `TimerSystem`, `TimerHandle`, owners, order,
  catching up, `unscaled`. Tests: a one-shot fires once at its time with
  time scale 0.5; a repeated one at a frame longer than its interval fires
  the right count; pause stops it; a destroyed owner is never called; a
  disabled owner waits and resumes with the remaining time.
- **T.2 The `Timer` component**: properties, `onTimeout`, methods, icon in
  the inspector. Tests: `autostart` fires in the Play copy and not in the
  edited scene; a persistent listener of `onTimeout` calls a method of
  another node.
- **T.3 Documents**: this section, frame (the place of timers in the frame).

## Scripts in Luau [planned, after MVP]

[Luau](https://luau.org) (MIT): Lua 5.1 with gradual typing, a built-in
sandbox and a language server.

- Every `.luau` script registers **its own component type**, named after the
  script, with the properties declared in the script. All of them create
  instances of one C++ class, `LuauScript`, bound to the script resource. The
  registry takes such types without a C++ class of their own, see
  [types and reflection](reflection.md).
- Bindings are generated from the registry: properties and methods of the
  types. One list serves Luau, the inspector and the API documentation. The
  same list produces type definition files for luau-lsp, which gives
  completion and type checking in an editor.
- Sandbox: Luau's own (`luaL_sandbox`: read-only globals, no `io` / `os`
  access), an interrupt callback against endless loops, an allocator with a
  memory limit.
- A Luau error disables the component and goes to the log, like a C++
  exception.
- Luau scripts are their own type module (`luau`), reloaded like `game.dll`.

## Visual scripts [planned, after 2D]

For those who do not write code: a script is built from blocks connected by
links, then attached to a node and works like a C++ or Luau script.

- A graph is a resource (a file of the project): blocks, their pins and the
  links between them. Execution links set the order of actions, data links
  carry values.
- Every graph registers **its own component type**, like a Luau script, with
  the properties declared in the graph. All of them create instances of one
  C++ class, `VisualScript`, bound to the graph resource.
- Entry blocks are the lifecycle calls (`onStart`, `onUpdate`, `onStop`) and
  later events (input, collisions).
- The block palette is generated from the same registry list as the Luau
  bindings: properties and registered methods of engine and game types. A new
  method of a type becomes a block without extra work.
- The engine interprets the graph. A limit on steps per call guards against
  endless loops; an error disables the component and goes to the log, like a
  C++ exception.
- Visual scripts are their own type module (`visual`), reloaded when a graph
  is saved.
- The graph editor is a panel of the editor; editing logic lives apart from
  ImGui, like the rest of the editor.

## Build and hot reload [done]

The loop: edit a script in the IDE, switch to the editor, and the editor
builds the game module, reloads it and puts the scene back with the same
field values; compiler errors show in the editor log.

### Decisions

- **Ninja decides what to rebuild.** The editor runs `cmake --build` when
  asked and does not track files itself: Ninja knows the real dependencies
  (headers found by the compiler, flags, configuration). When there is
  nothing to do the build is an almost empty check, and the module is
  reloaded only if the built library changed.
- **The engine is a relocatable CMake package.** The engine build puts an
  SDK next to the program (`sdk/`: public headers, the import library,
  `yqloraConfig.cmake`); every path in it is computed from the location of
  the package, never written absolute, so a moved editor still builds
  projects. `find_package(yqlora)` gives `yqlora::yqlora`, and
  `yqlora_add_game_module(<target> <sources>)` holds every rule of a game
  library. The SDK carries glm but not ImGui: game code draws through the
  engine, not through the editor's toolkit.
- **CMake names the result.** The package records `$<TARGET_FILE>` of the
  game module in the build folder; the editor reads the path from there and
  never builds a library name itself.
- **The toolchain reproduces the ABI of the editor.** The engine build
  records its toolchain: C and C++ compilers with id and version, Ninja,
  architecture, the MSVC runtime library and how to set up the Visual
  Studio environment (`vcvarsall`) for MSVC. The project build uses that
  record, so a script is built like the engine it is loaded into. The
  configuration (Debug, Release; project settings, section Scripts) changes
  optimization only, never the runtime library. Overrides live in the
  editor settings (`editor.json`): paths are per machine and stay out of
  git. A changed toolchain makes a fresh build folder, never a reuse of the
  old `CMakeCache.txt`.
- **Each load is a fresh copy.** The built library is copied to
  `.build/reload/game_<n>` before loading, so a loaded file is never
  overwritten (Windows locks it) and every reload is visible on disk. Copies
  that are not loaded are removed when the project opens or closes. The
  copies, loading and unloading are `GameLibrary`; what to do with the scene
  and the edit history around a reload is `EditorSession::reloadGameModule`.
- **The log is a buffer, not callbacks into the interface.** Subscribers of
  the log are called outside its lock and unsubscribe through an owning
  handle; the build writes into a thread-safe `LogBuffer` from its thread,
  and the editor takes new entries on the main thread.
- **Process, diagnostics and log are three parts.** `platform::Process`
  runs a program and reads stdout and stderr as they come (UTF-8, exit code,
  crash, termination, no blocking of the main thread);
  `CompilerDiagnostics` turns a line of GCC, Clang or MSVC into a
  structured entry (file, line, column, level); the log only stores
  entries.
- **One build at a time.** A request during a build sets one flag; however
  many requests came, one more build runs after the current one.

### The reload

After a successful build whose library changed (`EditorSession` owns the
order):

1. Stop Play; finish the current edit.
2. Serialize the edited scene into memory while the old module is still
   loaded, so every script field is read by its own code. The file on disk
   is not touched, so "unsaved" stays true or false as it was.
3. Clear the edit history: commands hold live objects (removed components,
   deleted branches) whose code goes away with the module. The log says so.
4. Destroy every component of the module (`GameModule::unload` checks that
   none is left in the scenes), remove its types, unload the library.
5. Copy the new library to `.build/reload/game_<n>`, load it, register its
   types.
6. Read the scene back from memory, restore the selection.

Failures:

- **The build fails**: the old module and the scene are not touched; the
  errors are in the log.
- **The new library does not load, or its entry function fails**
  (throws): the registration is rolled back (`removeModule`, the library
  unloads), so no half-registered types stay; the scene comes back with
  placeholders (`MissingComponent`) holding all data, and comes alive with
  the next good build. Reading the scene back is all or nothing (the tree is
  built aside and set only when complete).

### Phases of step 3

1. Log panel: log subscribers outside the lock with an owning handle,
   thread-safe `LogBuffer` (bounded, levels, source, counters) taken on the
   main thread, the Log panel (level filter, search, clear, copy, follow the
   end), the error counter in the main menu **[done]**.
2. The SDK and `scripts/`: relocatable `yqloraConfig.cmake` and
   `yqlora_add_game_module`, the toolchain record; the project template with
   `scripts/CMakeLists.txt`, `scripts/Module.cpp` (the entry function) and an
   example script; `scripts/` is not a resource; "Create scripts folder" for
   an existing project; `examples/` moves to `yqlora_add_game_module`; CI
   builds the template against the fresh engine on Linux and Windows
   **[done]**. The SDK is `bin/sdk/` (`cmake/Sdk.cmake`), the record is
   `sdk/toolchain.json`; the package refuses another compiler family and
   warns about another version. The template lives in
   `assets/templates/scripts/` with the `Pulse` script; the
   `yqlora_scripts_template` test builds it with the engine's compilers and
   flags. The menu item comes with phase 5.
3. The build: `platform::Process`, `CompilerDiagnostics`, `ScriptBuild`
   (configure only when needed, a fresh folder on a toolchain change, one
   build and one pending flag); Build (Ctrl+B), a build when the editor
   window gets focus and when a project opens; the Scripts section of the
   project settings **[done]**. A build nobody asked for (focus, project
   open) writes nothing to the log when there is nothing to build; the
   build folder is `.build/<configuration>-<fingerprint>`; on Windows the
   process runs without a console window inside a job object, so stopping
   kills the whole tree. Ninja on Windows does not find files by paths
   outside ASCII and would rebuild everything every time, so CMake gets
   short (8.3) paths (`platform::asciiPath`); without short names the build
   warns. Not done yet: running CMake through `vcvarsall`
   for an engine built with MSVC (the build reports it); the menu comes
   with phase 5.
4. Hot reload as above; the loaded copies in `.build/reload/`; the rollback
   of a failed registration. `gameLibraryPath` and the fixed
   `.build/game.dll` of step 2 go away: the path comes from CMake
   **[done]**. The rollback runs after the `catch`: a library must not be
   unloaded while its own exception is still being handled (on Windows that
   crashes). On Linux GCC builds the module with `-fno-gnu-unique`,
   otherwise `dlclose` keeps the old version loaded.
5. Interface: the Scripts menu (Build, Open scripts folder, Create scripts
   folder), the build state in the status bar, translations **[done]**. The
   editor has no status bar, so the build state sits in the main menu next
   to the log counters: "Building scripts..." while a build runs, a red mark
   after a failed one (a click opens the log).
6. Documents: this one, project (`scripts/`, `.build/`), project settings
   (Scripts), editor (log, build), the first script guide without copying,
   README **[done]**.
7. Scripts are files of the project, not a folder apart (after a review of
   phases 2 and 5): a C++ script is created in Files (Create resource > C++
   script, from `assets/templates/cpp-script/`) and lives anywhere in the
   project next to scenes; sources are shown in Files but are not resources
   (`Project::isSource`, no `.meta`). Build files come from the SDK
   (`sdk/game-project/`), scripts register themselves
   (`YQLORA_REGISTER_SCRIPT`), and the folder `scripts/` with `Module.cpp`
   and the menu items for it go away. A new project starts with `scenes/`,
   `scripts/` and `assets/` **[done]**.

Tests, besides the unit tests of every part: a change of a header only
rebuilds the module (a real CMake build of a temporary project); a source
changed during a build runs one more build; a library whose entry function
throws leaves no types and no components; values of script fields are the
same after several reloads and a save; after an unload no component of the
module is left in the scenes and no type of it in the registry.

Scripts are found by the build (a CMake glob over `.cpp` files), never by
reading C++: they register themselves, so nothing has to parse sources.

A component whose type is missing (the game failed to compile, a script was
deleted) is kept as a placeholder holding its raw data, shown in the inspector
as missing, and written back unchanged on save; after a reload
`reviveMissing` brings it back. Otherwise saving the scene would silently drop
the data **[done]**.
