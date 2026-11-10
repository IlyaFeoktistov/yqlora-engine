# Prefabs

[Русская версия](prefabs.ru.md)

A prefab is a branch of nodes saved once and placed many times: a door, an
enemy, a lamp with its light. A change of the prefab reaches every copy, and
every copy keeps its own changes. Objects created by a game while it runs
(a shot, an enemy wave) are prefabs too.

Status marks: see [README](README.md).

## Files [done]

- **Two kinds of file, one format.** A scene (`.yscene`) is a level, the
  place where objects stand; a prefab (`.yprefab`) is an object made to be
  placed many times. Both hold the same node tree: one loader reads it and
  one serializer writes it (`SceneSerializer`, branches by
  `SceneBranches`); the kind only tells what the file is for. A prefab goes
  into scenes and other prefabs; a scene goes nowhere, so a level cannot be
  dropped into a level by mistake. A piece of a level needed elsewhere (a
  room) is saved as a prefab.
- **A model is a prefab too**: a glTF file is read as a tree
  (`modelBranch`) and is never written. The ID of a model node is derived
  from its key in the file (`node:LeftDoor`), the ID of a component from the
  node and the type, so a re-export with the same names keeps references
  and overrides. Material slots from the file are prefab values: a slot
  changed in the scene is an override. A built-in mesh is placed the same
  way, as a node with a `MeshRenderer`.
- **A prefab field** (`Ref<Prefab>`) takes prefabs and models, never scenes
  (`kinds::fitsKind`).

## Instance [done]

- **Kept as a reference and overrides, not a copy**: the prefab
  (`ResourceId`), the instance ID and what this instance changed. In memory
  the instance is expanded into ordinary nodes, so scripts, systems, the
  renderer and the editor see only nodes.
- **Node links**: every node of an instance knows where it comes from
  (`Node::prefabLink()`: the instance ID and the node ID in the prefab); the
  instance root holds its data (`Node::prefabInstance()`,
  `PrefabInstance`): the prefab, the instance ID, the tree it was expanded
  from and the overrides without a target. Nodes added to the instance have
  no link.
- **IDs are derived**, not random: the ID of a node or a component of an
  instance is `deriveId(instance ID, ID in the prefab)`, the same on any
  machine and at every load. So a reference from outside into an instance (a
  `NodeRef` to a character's hand), the edit history and the selection
  survive a change of the prefab and a reload. A component ID is unique
  among the components of its node, so an override of the second of two
  scripts of one type finds exactly that one.
- **References inside the prefab** (`NodeRef`, `ComponentRef`, also in lists
  and records) are translated by the same function: in every instance they
  lead to its own nodes. References outside and to added nodes do not
  change.

### Format

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

`id` is the instance ID. `node`, `component` and `parent` are IDs in the
space of the prefab, never the derived ones: they stay the same in every
instance. Added nodes and components are written whole, as in a scene, with
their own IDs; references in values to nodes of the instance are in the
prefab space too. Empty kinds of overrides are not written.

## Overrides [done]

- **What an instance may change**:
  - a property of a node or a component: the whole value, by property name
    (a list, such as material slots, is one value);
  - an added node under any node of the instance (it becomes the last
    child), an added component;
  - a removed node or component of the prefab.

  A prefab node inside an instance is never moved to another parent.
- **The root's own values**: the name and the values of the root's built-in
  components (position, rotation, scale) always belong to the instance and
  are written as overrides, even when they match the prefab; the parent of
  the root is its own too.
- **When they are computed**: when the scene is written, the instance is
  compared with the tree it was expanded from (`computeOverrides`); when it
  is read, the overrides are applied to the current prefab data
  (`applyOverrides`), then the IDs are derived (`expandPrefab`). The tree
  for comparing is brought to the form a scene writes, so a property missing
  from the prefab file does not become an override.
- **An override without a target** (the prefab lost that node) stays in the
  file and in `PrefabInstance::unused`; it is never applied to another node
  and works again if the target comes back. The inspector shows a count with
  "Remove unused".
- **A prefab that does not expand** (no file, not a prefab, a cycle): a
  placeholder node named after the root in the record stands in place of the
  instance, and the record is written back as it was, so the scene loses
  nothing. Such an instance inside a prefab is dropped with an error in the
  log.

## Nesting and variants [done]

- A prefab may hold instances of other prefabs. A nested one is expanded
  from its current data when the outer one is expanded (`resolvePrefabs`),
  so a change of the inner prefab reaches every outer one. Nodes of a nested
  prefab get IDs in the space of the outer one (`deriveId` at every level);
  an override of the outer instance may target a node of the nested one.
- **A variant** is a prefab whose root is an instance of another prefab with
  overrides. It falls out of nesting; the editor only adds "Create variant".
- **A cycle** (a prefab that contains itself, directly or through others) is
  an error of expanding, not a hang: the chain of prefabs being expanded is
  checked at every level. The editor does not let one be made: a drop and an
  apply to a prefab that would make a cycle are refused (`prefabContains`).

## Prefab data [done]

- **A resource**: the loader of `.yprefab` and `.yscene` gives `PrefabData`
  and `SceneData` (the parsed tree in the file format, with its IDs), which
  live in `ResourceCache` with a revision like any resource. Nodes are built
  from them by whoever places the prefab.
- **The expanded tree** (nested ones expanded, values as a scene writes
  them) is cached (`PrefabBases`, in the engine services): expanding a
  prefab again neither parses nor normalizes it again. The tree is valid
  while none of its files (the prefab and nested ones, by the revisions of
  the resource cache) is reloaded and the types in the registry did not
  change (`TypeRegistry::revision`).
- **An instance holds its data** (`shared_ptr`): a reload of the prefab does
  not free it, and overrides are computed against what the instance was
  expanded from.

## In a game [done]

- **`scene().instantiate(prefab, parent)`** returns the root of a new
  instance of a prefab or a model (a new random instance ID) as the last
  child of `parent`; a scene is refused (`nullptr` with the reason in the
  log). The nodes take part from the next frame, like other nodes created
  during a frame. The same path (`SceneBranches::instantiate`) makes the
  instance of a drop in the editor. This is for the rare: a wave enemy, an
  item.
- **A prefab pool** (`PrefabPool`, a component) is for what comes and goes
  often: bullets, shells, hit effects. Fields: the prefab, the initial size,
  what to do when empty (`Grow` makes one more, `Refuse` hands out nothing).
  Instances are made at the start of the game (the pool's `onStart`) or on
  the first acquire, disabled, under the pool node; in an edited scene the
  pool is empty.
  - `acquire(parent)` hands out a free one: enables it and moves it to
    `parent` (during a frame at its end, like `Scene::setParent`).
  - `release(node)` takes it back: the properties of its nodes and
    components return to the prefab values (the root position too), the
    node is disabled and goes back under the pool. An instance that gained
    or lost nodes or components during its life is replaced with a new one.
    A node not from the pool is refused.
  - Script state that is not among its properties is reset by the script
    itself in `onEnable` or `onDisable`: they arrive as for any enabling.
  - `available()` and `total()`: free ones and all.

  Acquiring and releasing neither create nor destroy nodes. The difference
  between `instantiate` and the pool is visible in the game code: the engine
  never silently turns one into the other.
- **The Play copy and hot reload** go through the file format, so instances
  stay instances with their overrides.

## Editor [done]

- **A drop** of a model or a prefab from Files into the tree or the viewport
  places an instance; a scene is not taken, and an open prefab does not take
  a prefab that contains it. Instance nodes have their own colour in the
  tree, the root has the prefab icon; inner nodes can be selected and
  edited.
- **The inspector** colours overridden properties and added and changed
  components; a property and a component have "Revert to prefab" and "Apply
  to prefab" in their menu. The instance root has a row with its prefab and
  the buttons "Open prefab", "Revert all", "Apply all", "Unpack" and the
  count of unused overrides. The root's own values are neither reverted nor
  applied.
- **Applying** writes the prefab file and rebuilds every instance in the
  open scene; in the applied instance those values stop being overrides. A
  model cannot be applied to: it is read-only.
- **"Open prefab"** (or a double click in Files) opens the prefab for
  editing in the scene window, as a scene is opened. A prefab without its
  own sky and sun is lit by preview lighting: the sun and environment of a
  new scene from a hidden editor scene (`PreviewLighting`, `extractFrame`
  with a fallback lighting scene); the prefab's own lights stay, nothing
  goes into the file, and a button on the scene window bar turns it off.
- **"Save branch as prefab"** (the node menu in the tree, or a node dragged
  onto a folder in Files) writes the branch into a `.yprefab` through the
  create window and puts an instance in its place, one undo step; references
  of the scene to the branch nodes move to the instance nodes, and the file
  stays after undo, like any created one.
- **Create** in Files offers "Prefab" (an empty `Node3D`), and "Create
  variant" on a prefab or a model; "Unpack" on an instance turns it into
  ordinary nodes with the same IDs.
- **Rebuilding**: a prefab changed (the file was reloaded, the open prefab
  was saved, the types changed), and the instances of the open scene are
  built again from their records. If the prefab file is gone, the instance
  stays as it was and is saved by its record.
- **History**: every action is one undo step. Revert, apply, unpack and
  saving a branch remember the scene before and after
  (`SceneChangeCommand`, with the prefab file when it is rewritten); the
  history keeps IDs, so it works across rebuilds.

## Where things are [done]

| Part | Where |
|---|---|
| IDs, `deriveId` | `scene/ObjectId.hpp` |
| Node link, instance data | `scene/PrefabLink.hpp`, `scene/PrefabInstance.hpp` (internal) |
| Expanding and overrides over branches in the file format | `scene/PrefabExpansion.hpp` (internal) |
| Writing and reading instances | `SceneSerializer`, `SceneBranches` |
| Cache of expanded trees | `scene/PrefabBases.hpp` (internal) |
| A model as a prefab | `import/ModelPrefab.hpp` (internal) |
| Creating in a game, the pool | `Scene::instantiate`, `scene/PrefabPool.hpp` |
| Editor actions | `editor/PrefabEditing.hpp`, preview lighting in `editor/PreviewLighting.hpp` |
