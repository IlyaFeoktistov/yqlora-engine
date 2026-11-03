# Plugins and the plugin manager [planned]

[Русская версия](plugins.ru.md)

The engine holds the subsystems every game is built on: scene, rendering,
physics, audio, the HUD, scripting. Plugins hold ready-made pieces built with
the engine: a camera that follows the player, a player movement script, a
door, a day and night cycle, editor tools, asset packs. A developer takes
what fits the game from the plugin repository instead of writing it again.

Status marks: see [README](README.md).

## What a plugin holds

- Scripts: components of its own module (C++, later Luau and visual
  scripts). A plugin adds components, not node types, as a game does (see
  [scene](scene.md)).
- Scenes ready to drop into a game, for example a camera node with its
  follow script already attached.
- Resources: meshes, textures, sounds, shaders, materials. A plugin may hold
  only resources (an asset pack).
- Editor tools in an editor module that is kept out of the built game:
  gizmos, panels, custom inspectors.
- A manifest: name, version, the engine version it needs, the plugins it
  depends on.

## A plugin in a project

- An installed plugin is copied into the project, so it is kept in version
  control with the game and the game may change it for itself. The manifest
  keeps the version it came from, for updates.
- A project turns its plugins on and off; only enabled plugins are loaded and
  go into the built game.
- The types of a plugin belong to its module and leave the registry with it
  (`TypeRegistry::removeModule`), like the types of `game.dll`.
- A plugin module may register scene systems and settings sections, which a
  game module cannot. Before such a module is unloaded, its systems leave
  every scene and its sections leave the project settings (their values,
  saved or not, are kept in memory); after it is loaded again, they are
  created anew with those values, so no object of an unloaded library is
  left **[planned]**.
- Removing a plugin whose types are used in scenes warns first; the
  components of missing types stay as placeholders with their data, like
  after a failed build of the game (see [hot reload](scripting.md)).

## The plugin manager

- An editor window: installed, available, updates; search, install, update,
  remove.
- Sources: the plugin repository (an index in a git repository), a git URL,
  a local folder.

## Groundwork in the code

- Types belong to a module and leave with it **[done]**.
- Loading a game module with its types (`GameModule`) **[done]**.
- Placeholders for components of missing types (`MissingComponent`) **[done]**.
