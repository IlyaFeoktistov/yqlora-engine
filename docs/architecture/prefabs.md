# Prefabs

[Русская версия](prefabs.ru.md)

A prefab is a branch of nodes saved once and placed many times: a door, an
enemy, a lamp with its light. A change of the prefab reaches every copy, and
every copy keeps its own changes. Objects created by a game while it runs
(a shot, an enemy wave) are prefabs too.

Status marks: see [README](README.md).

## Decisions [planned]

The design is the established one for prefabs with overrides, nesting and
variants; nothing here is new.

- **Two kinds of file, one format.** A scene (`.yscene`) is a level, the
  place where objects stand; a prefab (`.yprefab`) is an object made to be
  placed many times. Inside both are the same tree of nodes, read by one
  loader and written by one serializer; the kind only says what the file is
  for. A prefab is placed into scenes and into other prefabs; a scene is
  never placed into anything, so a level cannot be dropped into a level by
  mistake. Files shows them with their own icons, Create offers both, the
  startup scene is chosen among scenes and a prefab field among prefabs. A
  model file (glTF) is a prefab too, read from the file and never written.
  A piece of a level wanted elsewhere (a room) is saved as a prefab.
- **An instance is stored as a link and overrides, never as a copy**: the
  prefab (`ResourceId`), the ID of the instance and what this instance
  changed. In memory an instance is expanded into ordinary nodes, so
  scripts, systems, rendering and the editor see only nodes; each node of
  an instance knows its link (`PrefabLink`: the instance and the node of
  the prefab it comes from).
- **IDs of instance nodes are derived**, not random:
  `deriveId(instanceId, sourceId)`, the same on every machine and every
  load. A reference from outside into an instance (`NodeRef` to the hand of
  a character), the edit history and the selection survive a change of the
  prefab and a reload. References inside the prefab are mapped by the same
  function. Nested prefabs apply it per level.
- **Components get IDs** like nodes (`ComponentId`, 64 bits, written with
  the component), derived in instances the same way. An override of the
  second of two scripts of one type finds it after the first is removed
  from the prefab; the index of a component in its node would not.
- **What an instance may change** (overrides):
  - a property of a node or a component: the whole value, by property
    name (a list, such as material slots, is one value);
  - an added node under any node of the instance, an added component;
  - a removed node or component of the prefab.

  The position, rotation, scale, name and parent of the instance root are
  always its own. Overrides are computed when the scene is saved, by
  comparing the instance with the prefab data it was expanded from, and
  applied when it is loaded.
- **An override whose target is gone** (the prefab lost that node) is kept
  in the file and shown in the inspector as unused, with "Remove unused
  overrides"; it is never applied to another node.
- **Nesting and variants**: a prefab may hold instances of other prefabs.
  A variant is a prefab whose root is an instance of another prefab with
  overrides: it falls out of nesting, the editor only adds "Create variant".
  A prefab that contains itself, directly or through others, is an error
  of loading and is refused by the editor when dropped.
- **Prefab data is a resource**: the loader of `.yprefab` and `.yscene`
  gives `SceneData`
  (the parsed tree with its IDs, not a `Scene`), kept in `ResourceCache`
  with a revision like any resource; nested prefabs are dependencies. A
  model gives the same shape from `ModelData`: the source ID of a model
  node is derived from its key in the file (`node:LeftDoor`), so a
  re-export with the same names keeps the overrides.
- **A changed prefab** (saved, changed on disk, a model exported again):
  every instance in an open scene is rebuilt, its overrides computed
  against the old data, then expanded from the new data with the overrides
  applied. The instance keeps the data it was expanded from until then
  (`shared_ptr`), so this never reads freed data. IDs do not change, so the
  selection and the edit history stay valid.
- **Unpack** turns an instance into ordinary nodes with the same IDs, one
  undo step; after it, changes of the prefab no longer reach them.
- **In a game**: `scene().instantiate(prefab, parent)` returns the root
  of a new instance (a new random instance ID); the nodes take part from
  the next frame, like other nodes created during a frame. The same call
  makes the tree that a drop into the editor makes.
- **Models in the scene become instances.** A model dropped into a scene is
  an instance of the model, not a copy of its nodes as now: an exported
  model updates every scene, and material slots filled from the file are
  the values of the prefab, so a slot changed in the scene is an override.

## Format

A scene holds an instance in place of a node:

```json
{
    "prefab": "4c1e07a95b3d2f60",
    "id": "da9dc314563ffd36",
    "properties": {"name": "Door", "position": [2.0, 0.0, 0.0]},
    "overrides": [
        {"node": "e51d90c4b7a2f386", "component": "07b3c2e9d14a6f58",
         "property": "materials", "value": ["9bf87a121dc2ad5c"]}
    ],
    "added": [{"parent": "e51d90c4b7a2f386", "node": {"type": "PointLight3D"}}],
    "addedComponents": [{"node": "e51d90c4b7a2f386", "component": {"type": "Rotator"}}],
    "removed": ["a3f0c2d1e4b59687"]
}
```

`node` and `component` are IDs in the space of the prefab (the source),
never the derived ones: they stay the same in every instance.

## Editor

- A model or a prefab dropped from Files into the tree or the viewport is
  an instance; a scene is not taken. Instance nodes have their own colour in the tree; their inner
  nodes can be selected and edited.
- The inspector marks overridden properties; a property, a component or
  the whole instance has "Revert" and "Apply to prefab". Applying writes
  the prefab file and rebuilds every instance.
- "Open prefab" (or a double click in Files) opens the prefab for editing
  in the scene window, as a scene is opened; "Save branch as prefab"
  (the context menu of a node, or a node dragged into Files) writes the
  branch into a `.yprefab` through the create window and puts an instance
  in its place, one undo step. Create in Files offers "Prefab" (an empty
  `Node3D`) and, on a prefab, "Create variant"; "Unpack" on an instance.
- Every action is one undo step; the history keeps IDs, so it works across
  rebuilds.

## Phases (a commit per phase)

### PF.1 IDs

- `ComponentId` with serialization (`id` of a component, written always,
  a missing one gets a new ID on load: formats stay at version 1).
- `deriveId(instance, source)` (splitmix64 of the two, never 0); a test of
  stability across runs and of no collisions on a million pairs.
- Tests: a component keeps its ID through save and load and through the
  Play copy; `NodeRef` and component IDs are not mixed up.

### PF.2 Prefab data

- `SceneData` (the parsed tree with node and component IDs, instance
  entries kept as entries) and its loader for `.yprefab` and `.yscene` in
  `ResourceCache`; the prefab kind in Files, Create and the icons;
  nested prefabs as dependencies; a cycle is the error of the resource.
- `ModelData` gives `SceneData` with source IDs from node keys.
- Tests: a prefab and a scene load as data with their IDs; a scene is not
  a prefab for a prefab field or a drop; a prefab inside a prefab
  is a dependency, its change reloads the outer one; a cycle of two files
  is an error, not a hang; a model gives the same tree a drop gives now.

### PF.3 Expanding and overrides

- `PrefabLink` on nodes, `expandPrefab` (derived IDs, mapped `NodeRef`,
  nested levels), `computeOverrides` and `applyOverrides`; the serializer
  writes an instance as its link and overrides and reads it back.
- Tests: save and load keep an instance as a link; each kind of override
  round-trips; an override of a nested prefab node; a reference from
  outside into an instance survives a change of the prefab; a property
  changed in the prefab reaches an instance that did not override it and
  not one that did; a removed source makes an unused override.

### PF.4 In a game

- `Scene::instantiate(prefab, parent)`, through `Script`; deferred like
  other tree changes; the Play copy and the hot reload keep instances.
- Tests: an instance made in `onUpdate` takes part from the next frame;
  100 instances of one prefab have distinct IDs and share mesh resources;
  a prefab and a model both instantiate, a scene is refused; hot reload keeps the
  overrides.

### PF.5 Editor

- Drops make instances (models included), the colour in the tree, the
  inspector marks, "Revert" and "Apply to prefab", "Open prefab", "Save
  branch as prefab", "Create variant", "Unpack", rebuild on a change,
  unused overrides.
- Tests (logic without the interface): each action is one undo step and
  undoes exactly; "Apply to prefab" changes the file and other instances;
  undo after a rebuild finds its nodes; a cycle is refused on drop.

### PF.6 Documents

This file, scene (links of nodes), project (`.yprefab`, the format of an
instance), resources (`SceneData`), editor, scripting (`instantiate`),
the guide "Prefabs", README.
