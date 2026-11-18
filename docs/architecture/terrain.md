# Terrain and vegetation

[Русская версия](terrain.ru.md)

Large outdoor ground sculpted and painted with brushes in the scene view,
with its own level of detail, collisions and holes; grass, flowers, bushes,
trees and rocks painted onto it and onto any surface, swaying in the wind.

## Decisions [planned, after the MVP, after wind]

- **The terrain is a component and a resource.** `Terrain` (on `Node3D`)
  places and draws a `.yterrain` resource: the heights, the layers with
  their weights, the holes and the detail kinds with their densities of
  one square area. What the maps mean (which layer a weight channel is,
  which detail a density map is) lives in the same file as the maps, so
  they cannot disagree. The resource knows
  nothing about the scene; the component knows nothing about brushes; the
  brushes are the editor. Several terrains side by side make a larger
  world; a brush that crosses an edge changes both, so the edges match.
- **The file.** `.yterrain` is one binary file: the `FileFormat` header
  and version, a JSON block with the size, resolution, layers and detail
  kinds, then the maps in tiles of `kTerrainTile` x `kTerrainTile`
  samples, each compressed by deflate (`stbi_zlib_compress` and
  `stbi_zlib_decode`, already in the build). One file is copied, moved and
  versioned like any resource; tiles let an edit write only what changed.
- **Heights are 16 bits.** 0 to 65535 over the range `minHeight` to
  `maxHeight`: a 1 km range has 1.5 cm steps, the same as the common
  height map formats, so 16-bit PNG and RAW maps from other tools are read
  and written as they are.
- **Level of detail: CDLOD.** A quadtree over the terrain picks for each
  node a level by its distance to the camera; all nodes draw one grid mesh
  with instancing, the vertex shader reads the height from the height
  texture and morphs vertices toward the coarser level near the edge of a
  level, so there are no cracks and no popping. It works with vertex
  texture fetch and the instancing of [particles](particles.md) (P.1) on
  the current RHI, without compute shaders. Normals come
  from the height texture in the fragment shader, so distant terrain keeps
  its relief.
- **Layers are materials of a kind.** A layer is a `.yterrainlayer`
  resource: albedo, normal and a packed mask (roughness, AO, height)
  textures, tiling in metres, an optional triplanar projection for steep
  slopes, and its physics material. A terrain blends up to `kMaxTerrainLayers` (8) layers by
  weight maps (two RGBA maps), with height blending (the higher texel of
  two layers wins at the border: stones stick out of sand). `Terrain` has
  a `material` on the built-in `Terrain` shader by default; a project's
  own terrain shader includes `<yqlora/terrain.glsl>` for the heights,
  normals and layers and shades them its way.
- **Collisions are Jolt's `HeightFieldShape`** from the same heights, with
  holes (Jolt's no-collision value) and Jolt's material per sample: the
  layer with the largest weight at a sample gives its physics material,
  so a car on sand and on asphalt behaves differently, and `Collision` and
  `RaycastHit` tell which layer was hit.
- **RHI additions**, each for all backends: texture arrays (the layers),
  writing a region of an existing texture (brushes change the GPU copy
  without making it again), the format `R16`.
- **Vegetation is two kinds of things.**
  - **Details** (grass, flowers, small stones): too many to keep one by
    one. The terrain keeps a density map per detail kind; instances are
    made only in cells around the camera, from the density and a hash of
    the cell (so the same grass stands in the same place every time), and
    drawn with instancing. Far away they fade out over `fadeStart` to
    `fadeEnd`. Without compute shaders the cells are filled on the CPU in a
    background job; with them later on the GPU, behind the same component.
  - **Instances** (trees, bushes, rocks, logs): few enough to keep. A
    `Foliage` component keeps the list of placed instances: a prefab, a
    position, a rotation and a scale. They are drawn with instancing by
    mesh and material, not as nodes; their colliders (from the prefab's
    colliders) are static bodies made in physics directly. Brushes put
    them on any surface (terrain, meshes, blockout), not only on terrain.
    Of a prefab an instance takes only its meshes and colliders; other
    components of the prefab are not run (a tree that has behaviour is a
    prefab placed as a node, not an instance), and the brush says so once.
  - **Colliders of instances** are one static compound body per cell of
    `kFoliageCell` metres, made by `Foliage` through `PhysicsWorld`
    directly: ten thousand trees are a few hundred bodies, not ten
    thousand components.
- **Distance.** Instances use the LODs of their meshes and are culled by
  distance per kind; impostors (billboards baked from several sides) for
  far trees come with the impostor baker, after this stage.
- **Wind** sways details and instances in the vertex shader by
  `windBend` (see [wind](wind.md)); the stiffness is in vertex colours of
  the model or a material parameter.

## Terrain (`Terrain`)

- `data` (the `.yterrain`), `material`, `lodDistance` (the range of the
  nearest level; each next level covers twice the range), `collision`,
  `castShadows`.
- The resource: `size` in metres and `resolution` (a power of two: 256 to
  4096; `resolution` + 1 samples per side), `minHeight`, `maxHeight`;
  `layers` (references to `.yterrainlayer`, in the order of the weight
  channels); `details` (detail kinds: a mesh or a billboard texture, a
  material, a size range, colour variation, a density scale, `fadeStart`,
  `fadeEnd`, `castShadows`, wind stiffness); the weight and density maps
  have their own resolution. The inspector of a `Terrain` shows and edits
  them as the resource's fields, through `ResourceEditing`.
- **Holes** are a bit map: a hole is not drawn and has no collision (caves,
  tunnel entrances, made with meshes).

## Brushes in the scene view

The Terrain panel shows the tools when a terrain is selected; one stroke
is one undo step (the tiles before the stroke are kept). Strokes change the
`.yterrain` in the cache; it is written to its file with the scene's Save,
through `ResourceEditing`, as edited materials are.

- A brush has `size`, `strength`, `falloff` (a curve) and an optional
  stamp texture (a mask: rocks, craters).
- **Sculpt**: raise and lower, smooth, flatten (to the height under the
  start of the stroke or a set one), set height, terrace, noise (from
  [noise](noise.md)).
- **Paint**: a layer (weights stay normalized), holes.
- **Details**: paint and erase density of a detail kind.
- **Instances**: paint and erase instances of the selected prefabs with
  density, random rotation and scale ranges, alignment to the normal,
  slope and height limits; a click puts one.
- **Rules of placement** for paint: a layer or an instance only where the
  slope, the height or another layer allow (cliffs get rock, grass not on
  roads).
- **Import and export**: 16-bit PNG and RAW height maps, PNG weight maps.
- **"Create > Terrain"**: an empty terrain with a new `.yterrain` next to
  the scene and the default layer.

## From code

- `Terrain`: `heightAt(x, z)`, `normalAt(x, z)`, `layerWeightsAt(x, z)`
  (what the ground is made of under a foot: sounds of steps, footprints),
  `holeAt`; `setHeights(region, values)`, `setLayerWeights`, `setHoles`
  for games that change the ground (craters, digging), applied to the GPU
  copy and the collision in the same frame; the changes of a running game
  are not written to the file.
- `Foliage`: `instances()`, `add`, `remove`, `removeInRadius` (cutting
  trees, clearing a path), `instanceAt(hit)`: a `RaycastHit` on an instance
  names the `Foliage` and its index.
- Details: `Terrain::setDetailDensity(kind, region, values)`, for grass
  burnt or mown in a game.

## Where the engine ends and the game begins

The engine gives the terrain with its level of detail, layers, holes and
collisions, the brushes, grass and placed instances with wind, and the
queries above. Footstep sounds by layer, trees that fall when cut, a
crater under an explosion and digging are the game, built on
`layerWeightsAt`, `Foliage::remove` and `setHeights`.

## Phases (a commit per phase)

### TR.1 The terrain resource and RHI additions

- `.yterrain` with tiles and deflate, its loader and writer,
  `.yterrainlayer`; texture arrays, region writes and `R16` in the RHI and
  the OpenGL backend.
- Tests: a written terrain loads back with the same heights, weights and
  holes; changing one tile rewrites only it; a file of an unknown version
  is refused; 16-bit PNG import and export give the same heights; a
  region write changes only that region of a texture (read back).

### TR.2 Drawing with CDLOD

- `Terrain`, the quadtree, the instanced grid with morphing, normals from
  heights, layers with height blending and triplanar slopes; frustum
  culling of quadtree nodes; shadows.
- Tests: the levels picked for a camera are within one step of each
  other across every edge (no cracks by construction); a terrain out of
  the frustum draws nothing; the number of draw instances stays within a
  bound for a 4 km terrain; weights at a point sum to one.

### TR.3 Collisions and queries

- `HeightFieldShape`, holes, physics materials by layer, the layer in
  `Collision` and `RaycastHit`; `heightAt`, `normalAt`, `layerWeightsAt`,
  `holeAt`, `setHeights`, `setLayerWeights`, `setHoles`.
- Tests: a ball dropped on the terrain stops at `heightAt` within
  tolerance; a ball falls through a hole; a ray gives the height and the
  layer under it; `setHeights` from code moves a body resting there in
  the next step; a ball rolls differently on a layer with ice.

### TR.4 Brushes

- The Terrain panel, sculpt and paint brushes, stamps, placement rules,
  edges of neighbouring terrains, import and export, "Create > Terrain".
- Tests (logic without the interface): raising by a brush of strength 1
  for one second raises the centre by its rate and leaves the outside of
  `size` untouched; smoothing lowers the variance; flatten levels to the
  height under the start; a stroke is one undo step that restores the
  tiles; a stroke over an edge keeps the edge heights of both terrains
  equal; painting keeps weights normalized.

### FO.1 Details

- Detail kinds, density maps, cells around the camera filled in a
  background job from density and a hash, instanced drawing, fading,
  wind; the detail brush.
- Tests: the same cell gives the same instances every time; zero
  density gives none; the cells follow the camera and the far ones are
  freed; the number of instances in a cell follows the density within a
  tolerance.

### FO.2 Instances

- `Foliage` with its list, instanced drawing by mesh and material, LODs
  and distance culling, compound static bodies per cell, the instance
  brush on any surface, `instanceAt`, `remove`, `removeInRadius`.
- Tests: painted instances stand on the surface and keep the slope and
  height limits; a ray hits an instance and names it; a removed instance
  loses its collider in the next step; 10 000 instances of one tree draw
  in one instanced draw per LOD and material and make one body per
  cell; the list survives save and load.

### FO.3 Documents

- "Terrain" and "Vegetation" guides, this file - [done].
