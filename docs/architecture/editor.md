# Editor

[Русская версия](editor.ru.md)

The editor is an application on the engine: the project manager, scene
editing and Play. Its logic lives in `editor/src` without ImGui, drawing in
`editor/ui`.

Status marks: see [README](README.md).

## Parts

- `EditorApp` assembles the editor: theme, settings, actions, panels.
- `EditorSession` holds the open project, the scene document, Play, the
  selection, the edit history (undo and redo of the scene and of the project
  settings), `SceneEditing` and `SettingsEditing`.
- `SceneDocument` is the edited scene and its file.
- **Unsaved changes** follow one rule for the scene and the project
  settings: a thing is unsaved while it differs from its file, however it
  was changed, through the history or not. The scene document keeps the
  scene as written to its file and compares after every edit, undo and redo,
  twice a second otherwise, and right before asking about unsaved changes;
  the settings compare their sections the same way. The one question is
  `EditorSession::modified()`; the history is only undo and redo **[done]**.
- `PlayMode` is the Play copy of the scene with its own services.
- Panels in `editor/ui` get an `EditorContext` every frame.

## Splitting [done]

- Editing (creating and deleting nodes, components, properties, transforms)
  lives in `SceneEditing` (`session.editing()`), which changes the current
  scene through commands. Project settings are changed by `SettingsEditing`
  (`session.settingsEditing()`); undo and redo are the session's, the history
  is shared.
- The session puts the open project into the engine services itself when it
  opens and closes a project; the window title is built by `windowTitle`,
  without ImGui.
- `EditorContext` gives panels an `EditorHost` instead of the whole
  `Application`: the renderer, the depth range, input, the language, the
  folder dialog. `RenderView` takes only the renderer.
- The scene document owns its scene; the application has none.
- The name of a resource in the inspector (a built-in resource, a path in
  the project) is resolved by `resourceLabel` in `editor/src`, without ImGui.
- The main scene of the project lives in the project settings, section
  General (see [project settings](project-settings.md)) **[done]**.

## Panels and property fields [done]

- Every panel opens its window through `EditorPanel::beginWindow`: one place
  for what all windows share.
- **Maximize**: any panel can fill the whole editor window and go back. A
  square button left of the close button (in the title of a floating window,
  in the tab bar of a docked one), Shift+Space for the focused panel, a
  right click on the tab or title, a double click on a docked tab, or View ->
  Maximize window. The maximized panel is a separate ImGui window over the
  work area; the dock layout is kept alive but not shown, so it comes back
  unchanged.
- **Property menu**: a right click on a property row in the inspector or the
  settings window. Every menu item says what it acts on:
  - "Reset value to default", "Copy value", "Paste value" act on the whole
    value: three numbers of a position, a colour, a list. The default is the
    value of a new object of the same type (`defaultValue`; a built-in
    component takes it from a new node of the owner's type). Copy puts the
    value on the system clipboard as the same JSON as in the files, so a
    position can be pasted into a scale; a string property takes any text.
  - A right click on one number of a vector (its X, Y, Z or W label or field)
    adds, above the whole value items, "Reset X", "Copy X", "Paste into X"
    for that number alone. "Paste into X" takes only a single number; three
    numbers are pasted with "Paste value".
  - Ctrl+C and Ctrl+V inside a field being edited copy its text, as in any
    text field.
  - Reset and paste are ordinary edits and are undone with Ctrl+Z.
- The label column of a property table fits the longest label, up to half
  the width; a clipped label shows its full text as a tooltip.
- **A record property** (the atmosphere of an environment) is a node that
  opens: its fields are rows of the same table, indented under it, with the
  whole width for values, and the label column fits them too. A property
  without a group after a group stands under a line, so it does not read as
  part of the group.
- **Fields are a file per widget**: `NumberFields` (dragged labels, numbers,
  vectors), `LayerFields`, `ChoiceFields` (enum, node, component type),
  `InputControlField`, `ResourceField`. `PropertyEditor` picks the field by
  the property type and hint and draws the row with its menu.
- **Icons are tables**: of node types (`nodeIcon`) and of resource kinds
  (`resourceIcon`); a new type or kind is a row. A point of the world on the
  screen of a view is `projectToViewport`, for picking and for the gizmos.

## Log [done]

- The engine log has subscribers (`log::subscribe`, internal): a listener
  is called outside the lock of the log, and the owning `log::Subscription`
  unsubscribes, waiting for a call already running.
- `LogBuffer` (`editor/`) keeps the last entries with their level and source
  (the engine, the script build). Any thread adds; the main thread takes new
  entries with `collect()` every frame, so the interface never sees the list
  change under it. `LogFilter` picks entries by level and text.
- The Log panel sits as a tab next to Files: level toggles with counters,
  search, clear, copy one entry or everything shown; while scrolled to the
  end it follows new entries. The main menu shows the error and warning
  counters on the right; a click opens the log.
- Build output goes to the same buffer with the source "build"; every line
  gets its level from `CompilerDiagnostics` (GCC, Clang, MSVC, the linker,
  CMake), so compiler errors count as errors.
- **Log file**: every start rewrites `yqlora.log` in the user data folder
  of the application (`SDL_GetPrefPath`: on Windows
  `%APPDATA%\Yqlora\Editor\`, on Linux `~/.local/share/Yqlora/Editor/`;
  the game has its own folder). By default only warnings and errors go
  there; `--debug` writes everything and opens a console. An error that
  stops the program is written as `Fatal: ...`, so after a crash the file
  tells why. Crash dumps are not the engine's: on Windows they come from
  Windows Error Reporting.
- A window added by a new version of the editor docks where the default
  layout would put it, unless `imgui.ini` already knows it
  (`dockNewWindows`).

## Building scripts [done]

- `ScriptBuild` builds the game module on its own thread, one build at a
  time; requests during a build give exactly one more. Triggers: Scripts >
  Build (Ctrl+B), Play, opening a project and the editor window getting
  focus. All but the first are quiet: when Ninja has nothing to do, the log
  stays empty. Play waits for the build and does not start if it failed;
  pressing it again while waiting cancels Play.
- Play brings the game view forward; if the scene view was shown before,
  Stop brings it back. An opened project starts with the scene view.
- A successful build whose library changed reloads the module
  (`EditorSession::reloadGameModule`, see [scripts](scripting.md)).
- The main menu shows "Building scripts..." during a build and a red mark
  after a failed one.
- The Scripts menu: Build. Scripts are created in Files: Create resource
  lists `newResourceTypes()` (folder, scene, C++ script, shader, sky
  shader, material; Luau and visual scripts join with their steps). A kind is one row of
  that table: its kind (for the icon), label, default name, the extension
  that shows a name is taken, whether the name is a C++ class name, and the
  function that creates it. Every kind asks for a name
  first (a free one is suggested) and is made by the Create button: Cancel
  leaves nothing behind. A C++ script's name is its class name; it is made
  from `assets/templates/cpp-script/`. A double click on a
  file that is not a scene opens it with the system (`platform::openWithSystem`).

## Resources in the editor [done]

- **Files** shows every file with the icon of its kind (the kind comes
  from the loader, `ResourceCache::kindOf`, files are not read; a scene is
  `SceneSerializer::kResourceKind`, see `fileKind`). Icons by kind are one
  table (`resourceIcon`) shared by Files, the resource field and the
  Create menu. A file whose loader has nested kinds (a model) opens into
  its nested resources (`nestedResources`). Any resource can be dragged.
  A click on an empty place clears the selection, so a new resource is
  made in the project root; a right click there opens the create menu.
  Files reads the folder again when the list of resource files changes
  (`ResourceDatabase::filesRevision`), whoever changed it.
- **The name window** (`NameDialog`): everything made, copied or renamed
  in the editor goes through one window with a name field and a confirm
  button; nothing is made before the button, and a failure (the name is
  taken) is said in the window.
- **Selection**: clicking a resource in Files selects it instead of nodes
  (`Selection::selectResource`), and the inspector shows the
  file; a nested resource is shown under the name of its key, with its
  file, read-only (`isResourceFile` is false). `ResourceInspector` shows
  what every file has (name, kind, the state and the message if the file
  is not loaded, from `inspectResource`)
  and then the inspector of its kind (`ResourceKindInspector`): a model
  (`ModelInspector`, data from `inspectModel`) shows its meshes with
  vertex, triangle and submesh counts, a texture (`TextureInspector`,
  `inspectTexture`) its size and a preview; materials and shaders are in
  [materials](materials.md#editor-done). A new kind is a new inspector
  and one line in the `ResourceInspector` constructor. Import settings
  (model scale; texture colour space, filter, wrap, mipmaps) are edited
  there and written to `.meta` by "Apply" (`applyModelImport`,
  `applyTextureImport`): the file is read again with a new revision. This is not in the edit
  history: the file and its `.meta` are not the scene.
- **Into the scene**: a model or a mesh dropped onto a node of the scene
  tree becomes its child, onto the empty place under the tree - a child of
  the root, into the viewport - at the point on the ground under the cursor
  (`placementPoint`). A model becomes a `Node3D` named after the file with
  the nodes of the model under it, a mesh - a `Node3D` with a
  `MeshRenderer` (`instantiateResource`); the slots of a mesh of a model
  get the materials of its file. One undo step
  (`SceneEditing::addResource`, `AddBranchCommand`).
- **A resource field** (`resourceField`): an icon and the name
  of the resource or "None (Mesh)"; clicking the field or the button on its
  right opens a picker with a search: None, built-in resources of the kind
  (from `builtinResourceTable()`), project assets (nested ones under their
  file, textures with a preview). The kind is the `resource(kind)`
  attribute of the property; what fits it is decided by `resourceChoices`.
  A resource dropped onto the field is taken if it is of the kind of the
  field, and a file with nested resources of that kind gives the first of
  them, so a model on a mesh field gives its first mesh
  (`resourceForField`). A nested resource reads
  "models/house.glb / Door".
- **Changes on disk**: when the editor window gets focus, the session walks
  the project folder again and reloads changed files
  (`EditorSession::refreshResources`).

## Snapping while moving [done]

A node is placed exactly on the grid, onto a vertex or onto a
surface, not by eye. It works in the manipulator and when a model is
dragged from Files into the viewport.

- **To the grid**: the magnet button on the viewport toolbar; Ctrl while
  dragging turns snapping on for that drag. Moving puts the node at the
  nearest point of the world grid (`snapToGrid`), rotating and scaling go by
  steps from the start of the drag (ImGuizmo steps). The steps are in the
  list next to the magnet: 1 m, 0.5 m, 0.25 m, 0.1 m or a custom one; the
  rotate and scale steps. The steps and the button states are editor
  settings (`EditorSettings::snap`), not project ones: they are a habit of
  the person.
- **To vertices**: with V held, the manipulator shows the vertex of the
  selected mesh closest to the cursor; it is dragged to a vertex of another
  mesh within 30 pixels of the cursor (`nearestVertex`). While dragging, the
  node changes directly; one edit goes into the history on release.
- **Onto a surface**: with Ctrl+Shift held while moving, the node is put at
  the surface point under the cursor (a ray into mesh triangles, its own
  branch skipped, `raycastSurfaces`); the button next to the magnet also
  turns its local Y axis along the normal (`alignUpTo`).
- A model dropped into the viewport lands on the surface under the cursor or
  on the ground, and on a grid point when the grid is on (`dropPoint`).
- The keys are held, not pressed, so they are constants of the manipulator,
  not menu actions; the button tooltips name them. They become rebindable
  together with the editor key settings.
- A snapped move is one edit in the history, as an unsnapped one.

## External code editor [planned, after step 5]

A file with an error opens at its line in the code editor the person uses.

- Editor settings (`EditorSettings::codeEditor`, a file in the user
  folder, a habit of the person, not of a project): the path of the
  program and the arguments, a template with `{file}`, `{line}`,
  `{column}` and `{project}`. Ready rows for common editors fill the
  template (`code --goto {file}:{line}:{column}`, `clion --line {line}
  {file}`, `subl {file}:{line}:{column}`); an empty path keeps opening with
  the system (`platform::openWithSystem`), which knows no line.
- A click on a log entry with `file:line` (shader errors, compiler errors
  from `CompilerDiagnostics`) opens it there; so does a double click on a
  `.yshader`, `.cpp` or `.hpp` in Files, at line 1.
- The arguments are passed as a list to the process (`platform::Process`),
  never through a shell, so a path with spaces or quotes stays one
  argument.
- When the Scripts tab comes (see [code editor](code-editor.md)), it is one
  more choice in the same setting.

### Phases (a commit per phase)

- **CE.1 Launching**: `platform::launchDetached` (starts a program without
  waiting for it, arguments as a list, no console window on Windows), the
  template with its placeholders, the ready rows. Tests: a template with
  `{file}` and `{line}` gives the right argument list for a path with spaces
  and quotes; an unknown placeholder is an error of the setting, not a
  silent empty string.
- **CE.2 Editor**: the first editor settings window (Edit > Editor
  settings; the toolchain overrides, now edited only in the file, move into
  it too) with the code editor setting; the log
  click, the double click in Files and "Open" of the shader inspector go
  through it; the system opening stays when the path is empty. Tests (logic
  without the interface): a diagnostic opens at its line and column; an
  empty setting falls back to the system.
- **CE.3 Documents**: this section, the guide "First script".
