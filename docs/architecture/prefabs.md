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
  always its own: the name and the values of the root's built-in components
  are always written as overrides, even when they match the prefab. An added
  node becomes the last child; a prefab node is never moved to another
  parent inside the instance. Overrides are computed when the scene is
  saved, by comparing the instance with the prefab data it was expanded
  from, and applied when it is loaded.
- **An override whose target is gone** (the prefab lost that node) is kept
  in the file and shown in the inspector as unused, with "Remove unused
  overrides"; it is never applied to another node.
- **A prefab that does not expand** (no file, not a prefab, a cycle): a
  placeholder node named after the root in the record stands in place of the
  instance, and the record is written back as it was, so the scene loses
  nothing. Such an instance inside a prefab is dropped with an error in the log.
- **Nesting and variants**: a prefab may hold instances of other prefabs.
  A variant is a prefab whose root is an instance of another prefab with
  overrides: it falls out of nesting, the editor only adds "Create variant".
  A prefab that contains itself, directly or through others, is an error
  of expanding and is refused by the editor when dropped.
- **Prefab data is a resource**: the loader of `.yprefab` and `.yscene`
  gives `PrefabData` and `SceneData` (the parsed tree in the format of the
  file, with its IDs, not a `Scene`), kept in `ResourceCache` with a
  revision like any resource. A nested prefab stays an entry of the outer
  one and is expanded from its own current data when the outer one is, so
  a change of the inner prefab reaches every outer one without making it a
  dependency of the file. A branch is written and read by the code of
  scene files (`SceneBranches`), so a prefab and a scene have one format.
  A model gives the same shape (`modelBranch`): the source ID of a model
  node is derived from its key in the file (`node:LeftDoor`), of a
  component from the node and its type, so a re-export with the same names
  keeps the overrides.
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
- **A prefab pool** is a separate path for objects that come and go often
  (bullets, shells, hit effects): instances are made ahead, disabled, and
  the pool hands out a ready one and takes it back by disabling it, with no
  expanding or destroying on the fly. `instantiate` is for the rare (a wave
  enemy, an item), the pool for the frequent; the difference is visible in
  the game code, the engine never silently turns one into the other.
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
    "overrides": [
        {"node": "8749596cb9467fd1", "property": "name", "value": "Door"},
        {"node": "8749596cb9467fd1", "component": "2ec4daa1615cfd6c",
         "property": "position", "value": [2.0, 0.0, 0.0]},
        {"node": "e51d90c4b7a2f386", "component": "07b3c2e9d14a6f58",
         "property": "materials", "value": ["9bf87a121dc2ad5c"]}
    ],
    "added": [{"parent": "e51d90c4b7a2f386", "node": {"type": "PointLight3D", "id": "..."}}],
    "addedComponents": [{"node": "e51d90c4b7a2f386", "component": {"type": "Rotator", "id": "..."}}],
    "removed": ["a3f0c2d1e4b59687"],
    "removedComponents": [{"node": "e51d90c4b7a2f386", "component": "5b2e81c0d97a4f13"}]
}
```

`id` is the instance ID; a prefab node or component in the instance has
the ID `deriveId(id, ID in the prefab)`. `node`, `component` and `parent`
are IDs in the space of the prefab (the source), never the derived ones:
they stay the same in every instance. Added nodes and components are
written whole, as in a scene, with their own IDs; references in values to
nodes of the instance are in the prefab space too. Empty kinds of
overrides are not written.

## Editor

- A model or a prefab dropped from Files into the tree or the viewport is
  an instance; a scene is not taken, and an open prefab does not take a
  prefab that contains it. Instance nodes have their own colour in the
  tree, the root has the prefab icon; inner nodes can be selected and
  edited, but not moved to another parent.
- The inspector marks overridden properties; a property, a component or
  the whole instance has "Revert" and "Apply to prefab"; added nodes and
  components are marked. The root's own values (name, position) are neither
  reverted nor applied. Applying writes the prefab file and rebuilds every
  instance; a model cannot be applied to, it is read-only. Unused overrides
  are shown as a count with "Remove unused".
- "Open prefab" (or a double click in Files) opens the prefab for editing
  in the scene window, as a scene is opened; "Save branch as prefab"
  (the context menu of a node, or a node dragged into Files) writes the
  branch into a `.yprefab` through the create window and puts an instance
  in its place, one undo step; references of the scene to the branch nodes
  move to the instance nodes, and the file stays after undo, like any
  created one. Create in Files offers "Prefab" (an empty `Node3D`) and, on
  a prefab or a model, "Create variant"; "Unpack" on an instance.
- A prefab changed (the file was reloaded, the open prefab was saved, the
  types changed): instances in the open scene are rebuilt; if the prefab
  file is gone, the instance stays as it was and is saved by its record.
- An open prefab without its own sky and sun is lit in the scene window by
  preview lighting: the sun and environment of a new scene from a hidden
  editor scene (`extractFrame` with a fallback lighting scene); the
  prefab's own lights stay. Nothing goes into the prefab file, and a button
  on the scene window bar turns it off.
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

- `PrefabData` and `SceneData` (the parsed tree with node and component
  IDs, instance entries kept as entries), one loader for `.yprefab` and
  `.yscene` in `ResourceCache`; `SceneBranches` (a branch written and read
  by the scene file code, with its IDs or new ones); the prefab kind in
  Files, Create and the icons; a field of `Ref<Prefab>` takes prefabs and
  models (`kinds::fitsKind`).
- `modelBranch`: the tree of a model or one of its meshes with IDs from
  node keys; the editor drop reads it with new IDs.
- Tests: a prefab and a scene load as data with their IDs, a file of a
  newer version is refused; a scene is not a prefab for a prefab field; a
  branch keeps its IDs or gets new ones; a model gives the same IDs on
  every build and after a re-export with the same names, a renamed node a
  new one.

### PF.3 Expanding and overrides

- `PrefabLink` on nodes, `expandPrefab` (derived IDs, mapped `NodeRef`,
  nested levels from the current data of each prefab, a cycle detected by
  the chain of prefabs being expanded), `computeOverrides` and
  `applyOverrides`; the serializer writes an instance as its link and
  overrides and reads it back. Tests also: a cycle of two files is an
  error, not a hang; a changed inner prefab reaches an outer instance.
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
- The expanded prefab tree (`PrefabInstance::base`) is cached by the prefab
  and the revisions of its file and nested files: a repeated `instantiate`
  neither parses nor compares the tree again.
- A prefab pool: a `PrefabPool` component (prefab, initial size, grow when
  empty or refuse) and the methods `acquire(parent)` and `release(node)`;
  instances are made at the start of the game (the pool's `onStart`) or on
  the first acquire if it comes earlier, disabled (in an edited scene the
  pool is empty); a handed out one is enabled, a returned one is disabled
  and goes back under the pool. Properties changed during an object's life
  are reset to the prefab values on return, so a handed out object is as
  new; `onEnable` and `onDisable` arrive as for any enabling. An instance
  that gained or lost nodes or components during its life is replaced with
  a new one on return. The same from code and from the inspector.
- Pool tests: acquiring and releasing neither create nor destroy nodes; an
  acquired instance has the prefab values after the changes of its past
  life; an empty pool grows or refuses as set; a node not from the pool is
  refused.

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
