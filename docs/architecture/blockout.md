# Blockout

[Русская версия](blockout.ru.md)

Building a test level fast, before any art exists: walls, floors, ramps
and stairs drawn and pulled right in the scene view, covered with grid
materials that show sizes in metres, with collisions at once. A blockout
is an ordinary part of a scene: it is played, saved, put into prefabs and
later replaced by models piece by piece.

## Decisions [planned, after cameras]

- **A shape is a component with parameters, not a mesh file.**
  `BlockoutShape` (on `Node3D`, `requiresComponent` `MeshRenderer`) keeps
  the kind of a shape and its sizes; the mesh is built from them when they
  change. A wall made 2 m longer stays a box with clean faces and the
  right texture scale, not a stretched mesh. Free modeling of vertices and
  edges is not part of the engine: real models come from a modeling
  program.
- **Meshes made by code.** `ResourceCache` takes meshes made at run time
  (`addRuntimeMesh(MeshData)`, `updateRuntimeMesh`, `removeRuntimeMesh`):
  they get IDs counted up in the top 2^32 values of the ID space
  (`kFirstRuntimeId`), are never in `ResourceDatabase` and never written
  to a file, and each change raises the revision, so the GPU copies and
  the cooked physics shapes, already keyed by ID and revision, work for
  them unchanged. New project IDs are drawn below that range; an old
  project ID inside it is as unlikely as two equal random IDs, which the
  engine already accepts. `MeshRenderer::setRuntimeMesh` draws
  such a mesh instead of its `mesh` property; like the overrides of
  materials it is run-time state and is not saved, and the inspector shows
  the mesh field read-only with the name of the component that set it.
  `BlockoutShape` sets it when it enters the scene and after every change.
  The same calls later serve terrain chunks, cloth and the games' own
  procedural meshes.
- **Collisions from what is drawn.** `MeshCollider` and `ConvexCollider`
  get `source`: `Mesh` (their mesh property, as now) or `Renderer` (the
  mesh the `MeshRenderer` of their node draws, a run-time one included;
  without a renderer the collider has no shape and says so once).
  A blockout shape gets a `MeshCollider` with `Renderer`: the collision
  is always the drawn shape, with nothing copied and nothing to keep in
  step. Without a `RigidBody` above, the collider makes a static body (see
  [physics](physics.md#bodies-and-colliders-planned-step-6)); a moving
  block (a platform) takes a kinematic `RigidBody`, which mesh shapes
  allow. A block that must fall and tumble is not a blockout: it takes a
  `BoxCollider` and a dynamic body.
- **Grid materials are shaders, not stretched textures.** The built-in
  `Blockout` shader draws the grid with anti-aliased lines (`fwidth`), so
  it stays crisp at any distance without shimmering, in one of two
  projections: `World` (triplanar by world position: the cells of
  neighbouring blocks line up) or `Uv` (by the mesh's UVs). Blockout
  meshes have their UVs in metres, so with `Uv` a cell is 1 m on any face
  and the grid moves with the block. A set of built-in materials on it
  (`Blockout Grey`, `Dark`, `Floor`, `Wall`, `Orange`, `Green`, `Blue`,
  `Red`) gives colours for meaning (walkable, obstacle, interactive,
  danger). Built-in textures `Grid 1m` and `Grid 1m Dark` (one cell per
  texture, thin lines every quarter, a mark in the middle; with UVs in
  metres a cell is 1 m) are there for one's own materials and models.
- **Editing goes through the tools of PH.9.** Face handles that change the
  size of a box collider (PH.9) are one widget, `BoxHandles`; blockout uses
  the same widget, so dragging a face, snapping and undo behave alike.
- **On the grid.** Sizes, drawing and handles snap to the grid of
  [snapping](editor.md#snapping-while-moving-done) when it is on; the
  steps are the editor's.

## Shapes

`BlockoutShape` with `kind` and the fields of its kind (sizes in metres,
at least `kMinBlockoutSize`):

- **`Box`** - `size` (x, y, z). Floors, platforms, pillars.
- **`Ramp`** - `size`; the slope rises along +Z. Ramps, roofs.
- **`Wedge`** - `size`, a triangular prism with the right angle at the
  bottom: sloped corners.
- **`Stairs`** - `size` and `steps`; `stepHeight()` is read from them, so
  a flight tests the `stepHeight` of the character controller.
  `closedSides`.
- **`Cylinder`** - `radius`, `height`, `segments`. Columns, round towers.
- **`Arch`** - `size`, `thickness`, `segments`: a doorway with a round top.
- **`Doorway`** - a wall `size` with a rectangular opening (`openingSize`,
  `openingOffset`): a wall with a door or a window in one piece.
- **`Wall`** - `points` (a line on the node's ground plane), `height`,
  `thickness`, `closed`: one mesh with mitred corners, so a room is one
  shape without gaps or overlapping faces.
- Every kind: `pivot` (`Bottom` by default, so the shape stands on what it
  is put on; `Center`).
- A new kind is a new enum value and a function that builds its mesh;
  nothing else changes.

## Tools in the scene view

- **"Create > Blockout > ..."** puts a shape of 1 m (a wall 4 x 3 x 0.2 m)
  in front of the camera with its `MeshCollider`, as any created object,
  one undo step.
- **Drawing a block** (the Blockout tool on the viewport toolbar): press
  on a surface and drag for the footprint (a rectangle on the plane of
  that surface, on the grid), release, move the mouse for the height,
  click to finish; Escape cancels. The new `Box` lies on the surface, with
  the tool's material.
- **Drawing a wall**: in the tool's Wall mode, clicks put the points of a
  line on the ground; Enter or a double click finishes, a click on the
  first point closes it. One `Wall` shape with the tool's `height` and
  `thickness`, one undo step.
- **Face handles** on a selected `Box`, `Ramp`, `Wedge` or `Stairs`:
  dragging a face moves it alone (the opposite face stays), on the grid.
  With Shift a drag **extrudes**: a new `Box` grows out of that face as
  long as the drag, with the same material, next to the old one: corridors
  and rooms built by pulling. The points of a `Wall` are dragged with
  their own handles.
- **Material by drop**: a material dropped from Files onto any mesh in the
  view sets the slot under the cursor (an editor feature for every
  `MeshRenderer`); the Blockout tool takes the last dropped material for
  its next blocks.

## From code

All properties of `BlockoutShape` are read and set from code; a change
rebuilds the mesh, and the collider follows it in the same step.
`bounds()` gives the local bounds of the shape. A game builds a level
from code (a generated maze, rooms of a roguelike) with the same shapes
the editor draws. Meshes made by code are open to games:
`resources().addRuntimeMesh(data)`, `updateRuntimeMesh`,
`removeRuntimeMesh`, `MeshRenderer::setRuntimeMesh`.

## Saving as a model

"Save as model" for selected shapes writes one `.gltf` with its `.bin`
(the writer of cgltf, already in the build), the grid materials as glTF
materials with their colours, a node per shape. An artist takes it into a
modeling program as the base of the real level; the file comes back as an
ordinary model.

## Where the engine ends and the game begins

The engine gives the shapes, the grid materials, the tools of drawing and
pulling, collisions and saving as a model: what a level designer of any
game needs to try a level before it has art. What the blocks mean (which
ones are walkable, where enemies stand, where a level starts) is the
game: its components on the nodes of the blocks or next to them.

## Phases (a commit per phase)

### BL.1 Meshes made by code

- Run-time meshes in `ResourceCache` with their ID range, new project
  IDs below it, `MeshRenderer::setRuntimeMesh`, `source` of `MeshCollider` and
  `ConvexCollider`.
- Tests: a run-time mesh draws and is not in `ResourceDatabase`; an update
  rebuilds the GPU copy and the cooked physics shape once; a scene with a
  run-time mesh on a renderer is saved without it; new project IDs never
  fall into the run-time range; a `MeshCollider` with
  `Renderer` collides with the run-time mesh and follows its update in the
  next step; a removed mesh leaves the renderer drawing nothing.

### BL.2 Grid materials

- The `Blockout` shader with `World` and `Uv` projections and
  anti-aliased lines; the built-in materials and grid textures; parameters
  `cellSize`, `lineColor`, `lineWidth`, `subdivisions`, `projection`.
- Tests (on OpenGL, read back): the built-in materials compile and are in
  the resource table; with `Uv` on a mesh with metre UVs the lines of a
  1 m material are 1 m apart on boxes of 1, 3.5 and 10 m; with `World` two
  touching boxes continue each other's lines; a moved node keeps its grid
  with `Uv` and changes it with `World`.

### BL.3 Shapes

- `BlockoutShape` with all kinds and pivots; "Create > Blockout".
- Tests: every kind builds a closed mesh with outward normals, UVs in
  metres and bounds equal to its size; changing a size rebuilds the mesh
  in the same frame; a closed `Wall` of four points has no gaps at the
  corners (each edge shared by two triangles); a sphere rolls down a ramp
  and a character walks up stairs whose step is below its `stepHeight`; a
  saved and loaded scene gives the same mesh.

### BL.4 Tools in the scene view

- The Blockout tool (a block and a wall), `BoxHandles` from PH.9 on
  shapes, extrusion with Shift, handles of wall points, material by drop.
- Tests (logic without the interface): a block drawn from two points and a
  height has that footprint and height on the grid; a wall of three
  clicks is one shape with three points and one undo step; a face drag
  keeps the opposite face in place; an extrusion makes a box adjacent to
  the face with the drag's length; a dropped material lands in the slot of
  the submesh under the cursor.

### BL.5 Saving as a model and documents

- "Save as model", a "Blockout" guide, this file - [done].
- Tests: a written `.gltf` loads back with a mesh per shape of the same
  bounds and the colours of the materials.
