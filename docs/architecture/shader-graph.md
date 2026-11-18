# Shader graph

[Русская версия](shader-graph.ru.md)

Making shaders without writing GLSL: blocks connected by links in a
graph, with a live preview. A graph becomes an ordinary shader, so
materials, the inspector and the renderer do not know whether a shader was
written or drawn.

## Decisions [planned, after the MVP]

- **A graph compiles to a `.yshader`.** A `.yshadergraph` file (JSON,
  `FileFormat` version 1) holds the blocks, their values and the links.
  Compiling a graph writes the GLSL text of a `.yshader` in memory (with
  the `Material` block, the annotations, the vertex and fragment parts)
  and gives it to the same `ShaderCompiler`; the result is a shader
  resource like any other. Materials pick a graph as their shader. Nothing
  after the compiler changes; a graph can be exported as a `.yshader` file
  to continue by hand.
- **One graph editor for shaders and visual scripts.** The model of a
  graph (blocks, typed pins, links, groups, comments, undo, copy and
  paste, checking a link) and its panel are one editor module,
  `editor/graph`, without ImGui in its logic, like the rest of the editor.
  The [visual scripts](scripting.md#visual-scripts-planned-after-2d) use
  the same module with their own blocks and their own meaning of a link
  (execution and data), the shader graph with its own (data only). Drawing
  goes through imgui-node-editor (MIT) behind the editor's widgets. The
  module is built with the shader graph, which comes first in the
  [order of work](README.md#order-of-work-planned).
- **Blocks come from a library, not a switch.** A block kind is a record:
  its pins with types, its parameters, and a function that writes its
  GLSL expression from the names of its inputs. The palette, the search
  and the checks read the library. A new block is one record; a project
  can add blocks as GLSL functions in a `.yshaderlib` file with annotated
  signatures.
- **Types.** Pins are `float`, `vec2`, `vec3`, `vec4`, `bool`, `texture2D`
  and `textureCube`. A smaller vector into a larger pin is widened, a larger
  one into a smaller pin takes its first components (with a mark on the
  link), anything else is refused when linking.
- **Outputs are the `Surface`.** The output block has the fields of
  `Surface` from `lighting.glsl` (base colour, metallic, roughness, normal,
  emission, occlusion, alpha, alpha clip) and the vertex part (a world
  offset, for wind and waves). A pin left free takes the value of
  `defaultSurface()`. Unlit and transparent modes are settings of the
  output block, not separate graphs.
- **Parameters are the material's.** A parameter block (a colour, a
  number with a range, a texture) becomes a field of the `Material` block
  with its annotations, so the material inspector shows it as for a
  written shader.
- **Only what is used is written.** Compilation walks back from the
  outputs: unlinked blocks write nothing, a block used twice is computed
  once (a local variable), constants fold. Blocks that must run per
  vertex (the wind bend, a vertex offset) feed the vertex part and pass
  values to the fragment part through varyings made by the compiler.
- **Errors point at blocks.** The compiler of a graph keeps a table of
  generated lines and the blocks that wrote them; a GLSL error of a line
  is mapped back through it, and the block is marked in the graph; the
  log shows the graph file and the block name.

## Blocks

- **Inputs**: UV 0, world and view position, normal and tangent in world
  and view space, the camera position and direction, the vertex colour,
  time (`time`, `unscaledTime`, from [wind](wind.md), WD.1), the screen
  position, the depth of the scene behind (for transparent surfaces and
  water; it needs depth sampling from
  [post-processing](post-processing.md), PP.1).
- **Math**: arithmetic, `lerp`, `clamp`, `saturate`, `step`,
  `smoothstep`, `pow`, `abs`, `fract`, trigonometry, dot and cross, length,
  normalize, remap, split and combine vectors, swizzle.
- **Textures**: sample 2D with a sampler mode, sample a normal map,
  triplanar, sample a cube, parallax (a height map).
- **Lighting helpers**: fresnel, normal blending, height blending of two
  layers.
- **Noise** (from [noise](noise.md), N.3): value, Perlin, simplex, cellular,
  fractal sums, the same as `noise.glsl`.
- **Wind** (from [wind](wind.md)): wind velocity and the bend for
  vegetation.
- **Utilities**: a sub graph (a `.yshadergraph` marked as a function, with
  its own inputs and outputs), a custom GLSL expression with typed pins,
  a branch by a condition (both sides computed, the result selected, as a
  GPU does anyway).

## The editor

- A graph opens in its own tab, like the code editor: the canvas, the
  palette with search (also by typing on the canvas), the parameters
  list, and a preview of a sphere, a cube or a plane with the scene's
  sky, rotated with the mouse.
- **Per-block preview**: each block with a colour or vector output shows a
  small preview of its value on the preview shape (compiled as a separate
  small shader that outputs that block), so a graph is read by looking.
- Saving recompiles; materials on the graph update in every open view.
- Undo, copy and paste between graphs, groups with titles and comments,
  reroute points on links, collapsing a selection into a sub graph.

## From code

A graph is a shader: materials on it are changed from code by parameter
names like any material
([materials from scripts](materials.md#materials-from-scripts-done)).
Graphs are not built from game code; tools that make shaders write a
`.yshader`.

## Where the engine ends and the game begins

The engine gives the graph, the compiler to an ordinary shader, the block
library and the editor. The shaders themselves (water, a dissolve, a
force field, a toon look) are the game's, built from blocks or added as
the game's own blocks in a `.yshaderlib`.

## Phases (a commit per phase)

### SG.1 The graph model and the editor module

- `editor/graph`: blocks, typed pins, links with checks, groups,
  comments, undo, copy and paste; the panel on imgui-node-editor; reading
  and writing `.yshadergraph`.
- Tests (logic without the interface): a link between unfitting types is
  refused and a fitting one widens; a cycle is refused; removing a block
  removes its links and one undo brings both back; a pasted selection gets
  new IDs and keeps its inner links; a graph survives save and load.

### SG.2 The compiler and the core blocks

- The block library, inputs, math, textures, parameters, the output
  block, compilation to GLSL with dead block removal, shared expressions,
  varyings, constant folding; error mapping to blocks; graphs as shader
  resources.
- Tests: the empty graph compiles to the default surface and draws as the
  standard shader would; a parameter becomes a `Material` field with its
  annotations and range; an unlinked block writes nothing; a block feeding
  two outputs is computed once; a vertex value used in the fragment part
  goes through one varying; a broken custom expression marks its block.

### SG.3 The editor tab and previews

- The tab with palette, search, parameters, the main preview and block
  previews; saving and live update of materials.
- Tests (logic without the interface): a block preview compiles a shader
  whose output is that block; saving a graph bumps the shader revision
  and the materials on it rebuild their pipelines.

### SG.4 More blocks, sub graphs and libraries

- Noise, wind, lighting helpers, triplanar, parallax, scene depth, sub
  graphs, `.yshaderlib`, export to `.yshader`.
- Tests: a sub graph used twice compiles its function once; a library
  function appears in the palette with its pins; an exported `.yshader`
  compiles to the same program as the graph.

### SG.5 Documents

- A "Shader graph" guide with a water and a dissolve example, the block
  reference generated from the library, this file - [done].
