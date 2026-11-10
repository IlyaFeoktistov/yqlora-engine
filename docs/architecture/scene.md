# Scene: nodes, components, resources

[Русская версия](scene.ru.md)

This is the contract for everything that lives in a scene: what is a node,
what is a component, the lifecycle, references and serialization. It changes
together with the code.

Status marks: see [README](README.md).

## Design decisions

1. **Games add components, not node types.** Node types come only from the
   engine. A game still creates nodes of engine types (`Node`, `Node3D`,
   `Camera3D`, ...) in code and attaches its own components to them. The type
   tree of nodes never changes when a game module is reloaded.
2. **A script is a component.** It cannot exist in the world on its own, only
   attached to a node. Every script class is its own component type, in C++,
   in Luau and in visual scripts alike.
3. **One component of a type per node by default.** A type opts in to
   repetition with `.allowMultiple()`.
4. **The core does not assume 3D.** Only 3D is implemented in the MVP, 2D comes
   after it, as a separate family next to 3D rather than inside it.
5. **Rendering, physics and audio are components on a node, not node types.**
   A mesh is `MeshRenderer` on a `Node3D`; a 2D sprite will be `SpriteRenderer`
   on a `Node2D`.

## Core concepts

| | What it is | Who defines types | Examples |
|---|---|---|---|
| **Node** | An object of the scene with its own role, one row in the scene tree | engine only | `Node`, `Node3D`, `Camera3D`, lights, UI controls |
| **Component** | An ability or behaviour attached to a node | engine and game | `MeshRenderer`, `RigidBody`, `BoxCollider`, `AudioSource`, scripts |
| **Resource** | A project file with a stable ID | engine (formats) | mesh, material, shader, texture, sound, scene, prefab, script source |

How to choose:

- The object makes no sense without it and is visible as a thing of its own in
  the tree (a camera, a light): a **node**.
- It is an addition that fits many different objects: a **component**.
- It is data shared by several objects: a **resource**, and a component refers
  to it.

## Node hierarchy

```
Node                     no position: a group, a root, a holder of scripts
|-- Node3D               built-in Transform3D (cannot be removed)
|   |-- Camera3D
|   `-- DirectionalLight3D, PointLight3D, SpotLight3D
|-- Environment          the sky, the ambient light and the atmosphere of the scene
|-- Control              built-in RectTransform, anchors   [planned, step 8]
|   `-- Label, Button, ...
`-- Node2D               built-in Transform2D              [planned, after MVP]
    `-- Camera2D, ...
```

- `Node2D` does not derive from `Node3D` and vice versa: the transforms have
  different meaning. They share only `Node`, the tree, the lifecycle and the
  editor infrastructure.
- A node class holds the properties of its role (field of view, light colour)
  and registers itself in scene systems in `onEnterScene` / `onExitScene`.
  **No game logic in node classes**: per-frame work of engine nodes belongs to
  their system. `onStart` / `onUpdate` / `onStop` are for components only
  **[done]**.

## Components

### Registration and rules [done]

```cpp
registry.registerComponent<MeshRenderer>()
    .requiresNode<Node3D>()               // which nodes it may live on
    .property("mesh", &MeshRenderer::mesh, &MeshRenderer::setMesh)
    .resource(MeshData::kResourceKind);   // which resources the field offers

registry.registerComponent<BoxCollider>()
    .requiresNode<Node3D>()
    .requiresComponent<RigidBody>();      // needs a RigidBody on the same node

registry.registerComponent<AudioSource>()
    .requiresNode<Node3D>()
    .allowMultiple();
```

- `requiresNode<T>()`: the owner must be `T` or derived from it.
- `requiresComponent<T>()`: the same node must have a component of type `T`
  or derived from it. The editor adds it together with the dependent one, in
  one undo step. A required component cannot be removed while something
  depends on it: its header has no close button and the tooltip names the
  dependents. Removing the dependents too is never implicit.
- `allowMultiple()`: several components of this type on one node. Each
  instance is still checked against `requiresNode` and `requiresComponent`.
- Rules are inherited: the rules of a base component type apply to derived
  ones.
- The base type comes from `YQLORA_TYPE(Class, Base)`, not from a second
  template argument of the registration, and registration does not compile
  if the class does not derive from it **[done]**.

`requires` is a C++20 keyword, hence `requiresNode` / `requiresComponent`.

### Who enforces the rules

The checks themselves are free functions in `ComponentRules.hpp` (`canAttach`,
`missingRequirements`, `dependentsOf`); everyone below calls them.

- The editor, through `EditorSession` **[done]**.
- The scene loader: a component on a node it does not fit is skipped with an
  error in the log; a missing required component is added with a warning
  **[done]**.
- `Node::addComponent` itself, for a node in a scene: it adds missing required
  components first, and a broken rule is an error
  of the program, so game code cannot bypass them either **[done]**.

### Built-in components

A spatial node creates its transform in the constructor (`Transform3D` for
`Node3D`). It cannot be removed and is not offered in "Add component".

### Rendering nodes and components [done]

What a scene shows is engine nodes and one component; how they are drawn
is in [materials](materials.md).

- **Lights** are nodes: `DirectionalLight3D` (the sun is one with the
  `sun` flag), `PointLight3D`, `SpotLight3D` derive from `Light3D`, which
  holds colour, intensity and the shadow mode with its strength, softness
  and biases. They register in `RenderSystem` in `onEnterScene`.
- **`Environment`** is a non-spatial node with the sky material, the source
  of ambient light and the atmosphere; the first active one in the scene
  works.
- **`MeshRenderer`** (only on `Node3D`, `requiresNode`) has a mesh and
  material slots, one per submesh: an empty slot draws with the default
  material. `castShadows` and `receiveShadows` are its own. Per-object
  overrides of slot values (`MeshRenderer::overrides`) are run-time state
  and are not saved; the slots are properties and are.

## Lifecycle

| Call | Where | What for |
|---|---|---|
| constructor | anywhere | defaults; no scene yet |
| `onEnterScene` / `onExitScene` | editor and game | register in scene systems (physics, audio, lights) |
| `onEnable` / `onDisable` | game only | the component starts or stops taking part: enabled and its node active in hierarchy **[done]** |
| `onStart` | game only | once, before the first phase call of the component |
| `onFixedUpdate(step)` | game only | physics, fixed step of game time **[done]** |
| `onUpdate(delta)` | game only | per-frame logic |
| `onLateUpdate(delta)` | game only | after all updates: follow cameras **[done]** |
| `onStop` | game only | release what `onStart` took |

### Phase lists instead of a tree walk [done]

- A scene keeps one list per phase (fixed update, update, late update) and
  calls only the components in it. A component gets into a list only if its
  type overrides that phase; the registry finds this out when the type is
  registered (`PhaseDetector` compares the declaring class of `onUpdate` and
  the others; `YQLORA_TYPE` makes it a friend of the class). No tree walk, no
  empty virtual calls.
- Order inside a phase: the **execution order** of the type (default 0, lower
  first), then the order in which components entered the list. A type sets its
  default in code (`.executionOrder(-100)`); the project settings can override
  it per type (see
  [project settings](project-settings.md)). One rule: the setting if present,
  else the default from code (`executionOrderOf`) **[done]**.
- A component added during a phase is called from the next phase on; a removed
  one is skipped at once and dropped from the list after the phase, so the
  lists never change under an iteration.

### Active and enabled [done]

- A node has `active` (its own flag) and `activeInHierarchy()` (it and all its
  parents are active). An inactive node switches off its whole branch:
  components get `onDisable`, leave the phase lists, renderers are not drawn,
  bodies leave the physics world.
- A component has `enabled`. It takes part in the game only while enabled and
  its node is active in hierarchy; `onEnable` / `onDisable` mark the change.
- There is no separate `visible` flag: hiding a mesh is disabling its
  `MeshRenderer`.
- A node has a **layer** (one of 32, names in the project settings). Camera
  masks and physics collisions filter by layers **[done]**; until the layer
  names come, the inspector shows numbers.
- An exception from a lifecycle call is caught: the component is disabled, the
  error goes to the log, the game goes on.
- `active` and `enabled` are ordinary registry properties (of `Node` and of
  the `Component` root type): the scene file, the inspector, undo and later
  the script bindings get them without extra code.
- `onEnable` comes before `onStart` and before the first phase after the
  component is enabled again; `onDisable` comes at once on disabling and
  before `onStop`. Re-enabling does not repeat `onStart`.
- A builtin component (`Transform3D`) cannot be disabled, as it cannot be
  removed: it has no `enabled` property.

### Deferred tree changes [done]

Destroying a node and changing its parent during a frame are applied at the
end of the frame, so no phase sees a half-changed tree. Nodes created during a
frame take part from the next one.

```cpp
scene.destroy(node);              // the node with its branch, the root too
scene.setParent(node, newParent, KeepWorld::No); // last child of newParent
```

- Outside a frame (editor, loading, tests) both apply at once; `inFrame()`
  tells which case it is.
- The queue refers to nodes by ID: a node gone with a destroyed branch is
  skipped, destroying twice is harmless.
- A move keeps the components running (no `onDisable` / `onEnable`, unless the
  new parent changes `activeInHierarchy`). What happens to the position is
  the choice below.
- A component enabled during a frame, like one of a new node, takes part from
  the next frame.
- `removeChild`, `setRoot` and `takeRoot` during a frame are a programming
  error: they would change the tree under the phases.

### Changing the parent: local or world position

Both are needed: a sword taken into a hand gets a new position relative to
the hand; a wagon hooked to a train must not move in the world.

- **Scripts** say it in every call, there is no default (any
  default would surprise someone) **[done]**:

  ```cpp
  scene.setParent(sword, hand, KeepWorld::No);    // local values stay, the node jumps
  scene.setParent(wagon, train, KeepWorld::Yes);  // stays in place in the world
  ```

- **The editor** asks when a node is dragged to another parent in the tree:
  "Keep the world position?" with a "Remember my choice" checkbox. The choice
  is an editor setting (a habit of the person, not a property of the game) and
  can be changed there. Alt while dropping does the opposite once. Undo returns
  both the parent with the place among siblings and the old transform
  (`SetParentCommand`). The choice is changed in "Edit > Moving a node in the
  tree" **[done]**.
- Keeping the world position is not always exact: under a parent with
  non-uniform scale and rotation the world matrix has a skew that position,
  rotation and scale cannot hold; the nearest values are taken.
- Nodes without a transform (`Node`) and parents that are not `Node3D` have
  nothing to recompute: the flag changes nothing there.

## Inside the scene [done]

`Scene` keeps the tree and the order of `advance`; the work is done by parts
it owns. Its public interface stays as it is.

- `ComponentLifecycle`: the phase lists, the start queues, `onEnable` /
  `onDisable`, calls guarded against exceptions.
- `TreeChangeQueue`: deferred destroying and changes of parent.
- `SceneSystems`: systems created from the registry, lookup by type, calls in
  the order of registration.
- `ComponentIndex`: components by type name.

## References, state and serialization

- A reference to a node is a `NodeId`, to a resource a `ResourceId`; both are
  property types. A raw pointer to another node or component may be kept only
  between `onEnterScene` and `onExitScene` and must be dropped on exit.
- **Registered properties are saved; everything else is run-time state.** The
  Play copy and hot reload go through serialization, so only properties
  survive them. That is the contract for scripts: whatever must survive a
  reload is a property.
- **A prefab instance** in the tree is ordinary nodes: each has a link to
  the prefab (`Node::prefabLink()`), the root has the instance data
  (`Node::prefabInstance()`). The IDs of its nodes and components are
  derived from the instance ID and the IDs in the prefab, references inside
  are translated; the file holds the instance as a reference to the prefab
  and overrides (see [prefabs](prefabs.md)) **[done]**.
- Serialization restores state, it does not manage lifetimes: subscriptions,
  system registrations and resources are released in `onStop` /
  `onExitScene` before a module is unloaded.

## Editor integration

- The inspector is built from properties and their hints (`PropertyHint`). A
  custom inspector for a type is added only when hints are not enough.
- "Add component" lists only the types that fit the node: `requiresNode`, the
  one-per-node rule and requirements that can be met **[done]**.
- Gizmos for a type live in the editor interface, `gizmos/` (like `CameraGizmo`); the engine
  knows nothing about them.
