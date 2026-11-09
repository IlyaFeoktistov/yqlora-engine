# Building and running a game

[Русская версия](build.ru.md)

Step 9 of the MVP: a game that runs without the editor. The editor builds a
folder that a player copies and starts: the player program, the game
module, the engine library and one package of resources.

Status marks: see [README](README.md).

## Decisions [planned, step 9]

The design is the established one for shipping a game from an editor:
resources are cooked into runtime formats through an import cache, packed
into one archive addressed by ID, and run by a ready player program per
platform; nothing here is new.

- **The player** (`apps/player`, `yqlora-player`): the engine library in
  game mode (`ApplicationMode::Game`), without the editor, ImGui and the
  shader compiler. It is built with the engine for every platform the
  engine supports and shipped with the editor, so building a game copies
  it, renamed to the game, instead of compiling it. The same program
  started with `--project <folder>` runs a project from its source files,
  for testing a build without packing it.
- **The game module is compiled for each platform** it ships on, as any
  C++ code: the editor builds for the platform it runs on, and a game for
  another platform is built on that platform (or by its CI). The Build
  section lists the target platforms; a target not buildable here says so.
- **Cooking and the import cache**: every resource is converted once into
  its runtime form, and the result is kept in the project cache
  (`.build/cache/`, by the hash of the source file, its import settings and
  the version of the cooker). The editor loads from the cache too, so a
  large model or texture is converted once, not at every start, and the
  build reuses the same results. A changed source or setting cooks again.
  The runtime forms:
  - scenes and prefabs: a binary form of the same tree (versioned like the
    JSON, which stays the source for the editor and version control);
  - models and meshes: `ModelData` and `MeshData` as they are in memory,
    so the player never parses glTF;
  - textures: with their mip chain, compressed for the GPU (BC7 for colour,
    BC5 for normal maps, BC4 for single channels; RGBA8 where the device
    lacks the formats), so the GPU memory of textures drops about four
    times;
  - shaders: SPIR-V with its reflection and the material layout, so the
    player needs no glslang (SPIRV-Cross stays in the OpenGL backend);
  - materials, physics materials, themes, fonts: their data as they are.
- **The package** (`<game>.ypak`): a table of contents by `ResourceId`
  (offset, size, kind, compression with zstd per entry) and the entries,
  read through memory mapping. IDs are baked in, so the player needs no
  `.meta` and no paths; a path table is kept for messages only.
- **What goes in**: everything reachable from the scenes of the build
  (references of properties, nested prefabs, materials, textures) and from
  the global nodes, plus folders marked "Always include" for resources a
  script finds by name. A resource missing from the package is an error of
  the build, not of the game at run time.
- **`ResourceSource`**: `ResourceCache` reads through a source: project
  files with the import cache in the editor and in `--project` runs, the
  package in a built game. Loaders and everything above them do not change.
- **Changing scenes**: `scenes().load(scene)` from a script switches the
  scene between frames, never in the middle of an update (the old scene
  stops, the new one starts on the next frame). `scenes().loadAsync(scene)`
  reads its resources on a worker thread and reports progress, so a loading
  screen can animate; loaders are functions of their input and run on any
  thread, the cache publishes their results on the main thread.
- **Global nodes**: prefabs listed in the project settings (General) are
  instantiated once at the start of a game and live across scene changes,
  for a game manager, music and a pause menu.
- **Saving game data**: `saveData().write(slot, value)` and `read(slot)`
  with a record or JSON, written atomically into the user data folder of
  the platform (`%APPDATA%`, `~/.local/share`); the editor and Play use a
  folder in the project cache, so tests of a game never touch the saves of
  a released one.
- **Display**: the Display section (resolution, window mode: windowed,
  borderless full screen, exclusive full screen, resizable, vsync from
  Graphics) is the default; a player's choice in the settings menu of the
  game is set through `display()` and kept in the user data folder.
- **The built folder**: the player renamed to the game, the game module,
  the engine library, SDL, the package, `licenses.txt` (the engine and its
  libraries) and, on Windows, the icon of the game in the executable.
  Logs go to a file in the user data folder; a crash shows where it is.

## Where the engine ends and the game begins

The engine builds, packs, loads, switches scenes, keeps saves and applies
display settings. The game decides which scenes exist and when to switch,
what a save holds and when it is written, what its settings menu offers.

## Phases (a commit per phase)

### B.1 The player

- `apps/player` in game mode, `--project` runs, the Display section and
  `display()`, logs to a file, the crash message.
- Tests: the player starts a project folder, runs ten frames headless and
  exits with code 0; a project without a startup scene says so and exits
  with an error code; display settings of the player override the project.

### B.2 Scenes, global nodes, saves

- `SceneRequests` with `load`, global nodes, `saveData()`.
- Tests: a scene switch requested in `onUpdate` happens after the frame,
  every component of the old scene gets `onStop`; a global node survives
  two switches; a save written and read back; an interrupted write leaves
  the old save.

### B.3 Cooking and the import cache

- The cooker per kind, the cache keyed by hashes, the binary scene form,
  texture compression with mips, SPIR-V with reflection; the editor loads
  through the cache.
- Tests: a second load of a model reads the cache, not glTF; a changed
  import setting cooks again; the binary scene equals the JSON one after
  loading; a compressed texture keeps its colour within the format error;
  a cooked shader draws the same frame as a compiled one.

### B.4 The package

- `.ypak` writer and reader, the dependency walk, "Always include",
  `ResourceSource` for files and for the package.
- Tests: a built package holds exactly the reachable resources; a
  resource referenced only from a global node is included; a missing one
  fails the build with its name; a scene loads from the package with no
  project folder present.

### B.5 Building from the editor

- The Build section (target platforms, scenes of the build, output folder,
  debug or release), the game module built in release, "Build" and "Build
  and Run" with progress, log and cancel.
- Tests (logic without the interface): the output folder has the expected
  files; a second build with no change reuses the cache; a failed module
  build stops with the compiler errors in the log.

### B.6 Loading in the background

- `loadAsync`, loaders on worker threads, progress.
- Tests: a scene with 50 textures loads while frames keep running; the
  progress goes from 0 to 1; switching away during a load cancels it
  without leaks (asan).

### B.7 Documents

This file, frame (scene changes, global nodes), resources (cooking, the
cache, the package), project (the cache folder), project settings
(Display, Build), scripting (`scenes()`, `saveData()`, `display()`), the
guide "Building a game", README.
