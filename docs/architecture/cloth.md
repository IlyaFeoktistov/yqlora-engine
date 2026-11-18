# Cloth and ropes

[Русская версия](cloth.ru.md)

Flags, curtains, capes, awnings, sails, nets and soft ropes that hang,
collide with bodies and characters and blow in the wind, fast enough for
many of them in a game scene.

## Decisions [planned, after the MVP, after wind]

- **Jolt soft bodies.** Jolt (already in the build) simulates soft bodies
  with edge, bend and volume constraints in its own threads, collides them
  with rigid bodies and the character's inner body, and lets vertices be
  kinematic (pinned) and moved each step. The engine adds what a game needs
  on top: pinning, wind, the bound to the render mesh and the budget of
  speed below. No simulation of our own.
- **Two meshes.** The simulation runs on a coarse mesh (`simMesh`, a few
  hundred vertices); the render mesh (any detail) is bound to it once:
  each render vertex keeps the triangle of the sim mesh nearest to it, its
  barycentric coordinates and its offset along the normal. Each frame the
  render vertices are placed from the sim triangles. So a detailed cape
  costs the simulation of a simple one. Without a `simMesh` the render
  mesh is simulated itself (a flag, a towel). The binding is computed when
  the meshes change and kept in the cache, not in the scene.
- **The drawn mesh is made by code.** The moved render mesh is a run-time
  mesh (see [blockout](blockout.md#decisions-planned-after-cameras)) that
  `Cloth` sets on its `MeshRenderer`; the renderer draws it as any mesh.
  Run-time meshes that change every frame get a dynamic RHI vertex buffer,
  written in place instead of being made again.
- **Pinning is per-vertex weights.** A weight 0 is pinned (it follows its
  anchor, kinematic in Jolt), 1 is free, between them a vertex is pulled
  toward its anchored place with that softness (Jolt's skinned constraints
  with a max distance). The weights are painted with a brush in the editor
  and kept in the component. An anchor is the cloth's node (a flag on a
  pole) or another node per group of vertices (a cape pinned to the
  shoulders of a character that is moved by animation later).
- **Wind** from [wind](wind.md): before each simulation step the force on
  a face is `0.5 * airDensity * dragCoefficient * area * (vRel . n) *
  |vRel . n|` along its normal, where `vRel` is the wind minus the face's
  velocity, plus a lift part by `liftCoefficient` across it; it is shared
  among the face's vertices as a change of their velocities (Jolt lets the
  vertices of a soft body be changed between steps). Air drag works
  without wind too (a cape lifts when running). `airDensity` is a
  setting of the Physics section, 1.225 kg/m3 by default.
- **The budget of speed.**
  - `maxVertices` per cloth and a project limit (`Physics.clothBudget`) of
    simulated vertices in the scene; over it, the farthest cloths freeze.
  - **By distance**: within `simulateDistance` a cloth runs with its
    `iterations`; up to `reducedDistance` with `reducedIterations`
    (Jolt's iteration count is per soft body); farther it freezes: its
    soft body is deactivated and the drawn mesh keeps its last pose,
    moved rigidly with the node. Distances are to the main camera and have
    a hysteresis, so a cloth on the border does not switch every frame.
  - **Off screen** a cloth runs with `reducedIterations`.
  - **Sleep**: Jolt's own sleep of a soft body that stopped; wind wakes it.
  - GPU simulation is Jolt's (its compute backends, off in the build now)
    and comes later behind the same component.
- **Ropes are cloth with one line.** `Rope` is a soft body of a line of
  vertices with edge and bend constraints, drawn as a tube (a run-time
  mesh with `radius` and `sides`). Two
  ends can be pinned to nodes or bodies. A rope that must pull bodies hard
  (a crane, a tow) is a [body chain](physics.md#body-chains-planned-step-6)
  or `DistanceJoint`; a soft rope hangs and wraps, it does not lift a truck.

## Cloth (`Cloth`)

On a `Node3D` with a `MeshRenderer` (`requiresComponent`).

- `simMesh` (empty means the render mesh), `pinWeights` (painted),
  `anchors` (groups of vertices and the node each follows).
- Material of the cloth: `stretchStiffness`, `bendStiffness`,
  `shearStiffness` (0 to 1, as Jolt's compliance turned around), `mass`
  per square metre, `damping`, `friction`, `thickness` (the collision
  radius of vertices), `selfCollision` (off by default: costly).
- Air: `dragCoefficient`, `liftCoefficient`, `windInfluence`.
- Budget: `iterations`, `reducedIterations`, `simulateDistance`,
  `reducedDistance`, `maxVertices`.
- `collisionMask` (the layers it collides with), `collideWithCharacters`.
- **Presets**: "Flag", "Curtain", "Cape", "Sail", "Net" set the fields
  above at creation; afterwards they are ordinary values.

## Rope (`Rope`)

- `length`, `segments`, `radius`, `sides`, a material, the start and end
  anchors (a node, a body, or free), `stretchStiffness`, `bendStiffness`,
  `mass` per metre, the air fields of `Cloth`, the budget fields.
- "Create > Rope" between two picked points, like "Create > Chain".

## From code

- All properties above; `pin(vertex, node)`, `unpin(vertex)`,
  `setPinWeight` (tearing a flag off its pole, cutting a rope's end);
  `addImpulse(position, impulse, radius)` (a hit on a curtain);
  `teleport()` (the cloth jumps with its node and does not drag across
  the world); `vertexPosition(i)` and `vertexCount()`.
- `Rope::point(t)` (a point at a share of the length) and `ends()`.
- Contacts of soft bodies are not reported as events: a game asks
  `overlap` with the cloth's layer when it needs to know.

## Editor

- **Pin paint mode** for a selected cloth: a brush of `size` and value
  paints weights on the vertices of the sim mesh, shown as colours on the
  mesh; one stroke is one undo step.
- **Preview**: the "Simulate" button runs the cloth in the edited scene
  without changing the scene (like a particle preview), with reset.
- A gizmo of the sim mesh as wires; the "physics.cloth" debug channel
  shows sim vertices, pinned ones in another colour, and the wind force.

## Where the engine ends and the game begins

The engine gives cloth and soft ropes with pinning, collisions, wind,
the binding of a detailed mesh to a coarse one and the speed budget. A
flag that tears when shot, a rope a player climbs, a sail that drives a
boat by its force: the game, from the API above and the wind field.

## Phases (a commit per phase)

### CL.1 Soft bodies and a simple cloth

- Soft bodies in `PhysicsWorld` (beside bodies, characters, joints and
  chains), `Cloth` on its own render mesh, pin weights and anchors,
  collisions, the run-time mesh output with a dynamic vertex buffer,
  `teleport`.
- Tests: a square cloth pinned at two corners hangs and comes to rest
  with the pinned vertices in place; a free cloth falls on a box and lies
  over it, not through; a pin weight between 0 and 1 holds the vertex
  within its distance; moving the anchor node moves the pinned vertices
  in the same step; `teleport` moves the whole cloth without stretching;
  a scene that does not run builds no soft bodies.

### CL.2 Wind, air and the budget

- Drag and lift by faces from the wind field, distance levels with
  hysteresis, the scene budget, off-screen iterations, sleep.
- Tests: a cloth pinned on one edge in a 10 m/s wind lifts to near
  horizontal and without wind hangs; drag without wind slows a falling
  cloth; a cloth beyond `reducedDistance` keeps its pose and its soft
  body is inactive; the budget freezes the farthest cloths first; a cloth
  on the border of a distance does not switch levels each frame.

### CL.3 Two meshes and ropes

- The binding of the render mesh to the sim mesh (computed once and
  cached), `Rope` with a tube mesh, "Create > Rope".
- Tests: a render vertex lying on a sim triangle follows it exactly; the
  binding is computed once per pair of mesh revisions; a rope of 2 m with
  both ends pinned 1.5 m apart sags and keeps its length within
  tolerance; a cut end (`unpin`) falls.

### CL.4 Editor and documents

- Pin paint mode, preview, presets, the debug channel; a "Cloth and
  ropes" guide, this file - [done].
- Tests (logic without the interface): a paint stroke sets the weights
  under the brush by its falloff and is one undo step; a preset sets its
  values and nothing else.
