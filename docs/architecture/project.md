# Project and file formats

[Русская версия](project.ru.md)

What a project folder holds, how its files are written and how a project
moves to a newer format. Settings sections are in
[project settings](project-settings.md), resource IDs in
[resources](resources.md).

Status marks: see [README](README.md).

## The project folder [done]

```
MyGame/
|-- project.yqlora          name of the project, format version, engine version
|-- settings/               one file per settings section
|   |-- general.json
|   |-- time.json
|   `-- ...
|-- scenes/                 starter folders of a new project; any layout works
|   |-- main.yscene         scenes, anywhere in the folder
|   `-- main.yscene.meta    resource ID of the file next to it
|-- scripts/
|   |-- Player.hpp          C++ scripts, anywhere in the folder, without .meta
|   `-- Player.cpp
|-- assets/                 other resources with their .meta
|   |-- house.glb           glTF models, pictures - anywhere
|   |-- house.glb.meta      file ID, IDs of its meshes, materials and
|   |                       textures, import settings
|   |-- wood.ymat           materials
|   `-- water.yshader       shaders of the project
`-- .build/                 build output, not in git
    |-- Debug-<fingerprint>/   CMake build folder of the game module
    `-- reload/             copies of the module the editor has loaded
```

- `project.yqlora` and `settings/` are not resources: the resource scan
  skips them (`Project::reservedNames`), the Files panel hides them and
  cannot rename or delete them.
- C++ sources (`Project::sourceExtensions`) are files of the project shown
  in Files, but not resources: nothing refers to them by ID, so they get no
  `.meta`. The game module is built from every `.cpp` outside hidden
  folders; a double click opens a source in the program the system assigns
  (the IDE).
- Hidden files and folders (starting with a dot) are skipped too, so the
  build output in `.build/` (`Project::buildDirectoryName`) is never a
  resource. The editor builds the game module there and loads a copy of it
  from `.build/reload/` (see [scripts](scripting.md)).
- The project is `Project` in the engine: it is needed by the editor and by
  the game runtime (step 9). Creating, importing and deleting a project folder
  is the editor's job (`ProjectSetup`, `ProjectManager`).

## Files [done]

All files of the engine are JSON, written with indentation so a diff in
version control reads well. Every file has `version`, the version of its
format. Shaders are the exception: a `.yshader` is GLSL text, and the
version it is written for (the shader profile) is in its `.meta`.

`project.yqlora`:

```json
{
    "engine": "0.0.1",
    "name": "MyGame",
    "version": 1
}
```

`settings/<section>.json` (see [project settings](project-settings.md)):

```json
{
    "values": {
        "gameName": "MyGame",
        "startupScene": "9bf87a121dc2ad5c"
    },
    "version": 1
}
```

`<resource>.meta`: the file ID, the import settings (an object of its
loader, defaults are not written) and the IDs of nested resources by key
(see [resources](resources.md)). A scene and a picture without settings
hold only the ID.

```json
{
    "id": "9bf87a121dc2ad5c",
    "import": {"scale": 0.01},
    "subresources": {
        "mesh:Door": "4c1e07a95b3d2f60",
        "material:Wood": "e51d90c4b7a2f386",
        "texture:Grain": "07b3c2e9d14a6f58"
    },
    "version": 1
}
```

The `.meta` of a shader holds the profile version in its import settings:
`"import": {"profileVersion": 1}`.

`*.ymat`: the shader, render state, values and textures of a material;
the format and the rules of values are in
[materials](materials.md#materials-done).

`*.yshader`: one GLSL 450 file with both stages; the annotations and the
engine library are in [materials](materials.md#shader-files-done).

`*.yscene` and `*.yprefab` (a scene and a prefab, one format): the root
node with its properties, components and children. A prefab instance stands
in place of a node as a record: the prefab, the instance ID and the
overrides (the format is in [prefabs](prefabs.md#format)).

```json
{
    "root": {
        "type": "Node3D",
        "id": "da9dc314563ffd36",
        "properties": {"name": "Root", "active": true},
        "components": [
            {"type": "Transform3D", "id": "07b3c2e9d14a6f58", "properties": {"position": [0.0, 0.0, 0.0]}}
        ],
        "children": []
    },
    "version": 1
}
```

- Properties are written by the type registry: every registered property of
  the type, under its name (see [reflection](reflection.md)). A missing
  property keeps its default value. An unknown property of a component is
  kept with the component and written back (`UnknownProperties`), and a
  component of an unknown type becomes a placeholder holding its JSON
  (`MissingComponent`), so adding or removing a field or a script in code
  neither breaks nor loses saved data (see [scripts](scripting.md)). Unknown
  properties of nodes are skipped: node types belong to the engine, and its
  format changes go through migrations.
- A component has its own ID (unique among the components of its node),
  so references to a component and overrides of prefabs find it; a file
  without one, or with an ID repeated on the node, gets a new one on load.
- 64-bit IDs (nodes, components, resources) are written as strings of 16 hex digits:
  many tools read JSON numbers only up to 2^53 and would spoil them silently.
- Enumerations are written by name, so the order of values in C++ can change.
- A binary scene format for the built game comes with step 9; JSON stays the
  source format for the editor and version control **[planned, step 9]**.

## Format versions [done]

- Each kind of file has its format (`FileFormat`): a name, the first version
  and the list of migrations. The current version is derived from the list:
  adding a migration raises the version. A migration declares the keys of the
  old version itself, so renaming a key later does not change old migrations.
- Reading a file says whether its version is current, older (can be
  upgraded), newer (saved by a newer engine) or unsupported (no version, or no
  migration from it).
- Until there are users, every format stays at version 1 without backward
  compatibility. When a format changes, it gets a migration and a test, never
  a silent read of the old shape.

## Upgrading a project [done]

- The project manager shows the engine version of every project and of the
  engine itself. A project of an older format opens only through the
  "Upgrade project" window; after the upgrade older engines cannot open it. A
  project of a newer engine or without a version does not open, with the
  reason.
- `upgradeProject` reads and migrates every scene and every settings file in
  memory first; if one cannot be read, nothing is written. Then it writes the
  settings, the scenes and, last, `project.yqlora`. While the project file is
  old the project is offered for upgrade again, and upgrading an already
  upgraded file changes nothing, so an interrupted upgrade is simply repeated.
