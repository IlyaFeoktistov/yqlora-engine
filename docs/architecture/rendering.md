# Rendering

[Русская версия](rendering.ru.md)

Cameras, views and how the scene reaches the renderer. The stages of the
render frame are in [the frame pipeline](frame.md).

Status marks: see [README](README.md).

## Render pipeline

How a frame gets from the scene to the screen. The renderer reads only the
snapshot made by Extract, never the scene.

### Today [done]

```
 SCENE (simulated, world matrices ready)
   RenderSystem: active cameras sorted by order, active lights
   Environment: the sky material and the atmosphere
   MeshRenderer: enabled components, their mesh and materials (from
                 ResourceCache), overrides, node layer
   Node3D: world matrices
        |
        |  extractFrame(scene, aspect)                    render/FrameBuilder
        |    extractViews, extractLights, extractDrawItems,
        |    extractEnvironment                           render/FrameExtract
        v
 FrameData (snapshot, nothing points back into the scene)
   views[]      one per camera: view and projection, clear mode,
                background (linear), culling mask, exposure and tone
                mapping, passes of the owner
   drawItems[]  one per submesh: mesh with its ID and revision, index
                range, world matrix, world bounds, material, overrides,
                layer, node id
   materials[]  each material once, with its shader and textures
   overrides[]  overrides of slots with their textures
   lights[]     up to 4 directional and 32 point and spot lights: world,
                linear colour times intensity (lux, candela), range, cone;
                the sun first, marked
   environment  the sky material (an index into materials) and the
                atmosphere
        |
        |  Renderer::render(frame)          -> window back buffer
        |  Renderer::render(frame, target)  -> RenderTarget (a texture)
        v
 SceneRenderer::draw: one RenderGraph per call       engine/render
   AtmospherePass the transmittance and multiple scattering tables, only
                  when the atmosphere changed
   for each view, in camera order, into the HDR target (kHdrColorFormat):
     prepareView  layer mask and frustum culling; opaque by pipeline and
                  material, nearest first; then transparent, farthest first
     AtmospherePass  the sky view and aerial perspective tables of the view
     view pass    clear: sky / colour and depth / depth only / nothing;
                  MeshPass: the opaque items through the view camera
                  with their materials (PBR, all lights of the frame, the
                  sun through the atmosphere, aerial perspective), times
                  the exposure of the view; drawSky on the far plane for
                  a Sky camera; MeshPass: the transparent items
   OutputPass     tone mapping of the main camera, then sRGB, from the HDR
                  target into the final target
   ViewPass*      passes of the view owners over the final target: the
                  Viewport adds BackgroundPass (the theme colour where
                  the scene drew nothing, exact, past tone mapping),
                  GridPass and OutlinePass (editor/render)
   graph.execute  orders the passes, makes the transient textures,
                  changes texture states, records RHI commands
        |
        v
 UiRenderer (ImGui), then present (swap buffers)
```

- **The game** (`Application`, game mode) does it once per frame in the
  `extract` and `render` stages of `FrameLoop` (see [frame](frame.md)), into
  the window.
- **The editor** has no scene of its own to draw in those stages. Each scene
  window (Viewport, Game, the camera preview) calls `extractFrame` with its
  own aspect and renders into its own `RenderTarget` during the interface
  stage; ImGui then shows the texture. The Viewport adds the editor camera
  view and its passes (grid, outline), with the exposure and tone mapping
  of the main camera of the scene.

### What the stages will hold [planned]

The stages above stay; later steps fill them: lights, materials, sky,
shadows and transparency in step 5, post-processing after the MVP,
Vulkan and D3D12 as new backends.

```
 Extract   -> FrameData (also lights, materials)
 Prepare   -> per view: culling by bounds and layer mask, sorting
              (opaque front to back, transparent back to front)
 Graph     -> passes declare what they read and write, the graph orders them:
              shadows -> opaque -> sky -> transparent -> post-processing -> HUD
              (+ passes the editor adds to its views: grid, outline)
 Renderer  -> records the passes as RHI commands     engine/render
 RHI       -> one interface over OpenGL, Vulkan, D3D12   backends/<api>
 Present
```

## Photorealism by default [planned, after the MVP]

The goal: the defaults are chosen so that the picture
looks like a photograph at once. Everything stays tunable: each effect has
its own parameters, which can be changed or turned off for a style of
one's own. It is a large stage after the MVP; the decisions of step 5 leave
room for it:

- **Already in step 5**, so nothing is redone: physical light units, the
  exposure of the camera and tone mapping (AgX by default), the physical
  sky and atmosphere with aerial perspective, ambient light and reflections
  from the sky (see [materials](materials.md#light-units-exposure-and-tone-mapping-done)).
- **Post-processing** in and before the output pass: auto exposure (one
  more mode of the camera exposure), bloom, temporal anti-aliasing (TAA),
  then depth of field, motion blur, colour grading.
- **Lighting**: reflection probes placed in the scene, ambient occlusion
  (SSAO), then global illumination.
- **Area lights**: a rectangle and a disc (computed with LTC); directional, point and spot lights exist from step 5,
  and so does the sun, a directional light with a flag.
- **Shadows beyond step 5** (which brings sun cascades, an atlas for spot
  and point lights, PCF softness): contact shadows, softness by light size
  (PCSS), shadows from transparent surfaces, ray tracing.
- **Clouds and volumetric fog** on top of the tables of the atmosphere.

The content and order of the stage are agreed separately, before it starts.

## The RHI stage [done]

Done before step 5, so that shaders, materials and lights are written once
for every graphics API. Only the OpenGL implementation is made now; Vulkan
and D3D12 come later. The interface carries no OpenGL assumptions, but has
only what the renderer needs today: nothing is added "for Vulkan" before
Vulkan itself.

### Where the line goes

| Engine (`engine/render`) | Backend (`backends/<api>`) |
| --- | --- |
| `Renderer`, passes (`MeshPass`), Prepare, the render graph | `rhi::Device`, `rhi::CommandList` of the API |
| GPU copies of resources by ID and revision (`GpuResourceCache`) | buffers, textures, samplers, pipelines of the API |
| shaders: compiling GLSL to SPIR-V, reflection | a shader module of the API from SPIR-V |
| what to draw and in which order | executing the commands |

- The RHI knows nothing about the graph. The graph is built over the RHI and
  turns into RHI commands, so each is tested without the other.
- ImGui is a separate integration of the backend (`UiIntegration`), not
  part of the RHI: it draws through the same API and device, but the RHI
  does not depend on it. It makes the `UiRenderer` and turns a texture of
  the device into a `UiTexture` for `ImGui::Image` (`Renderer::uiTexture`,
  `RenderTarget::uiTexture`); the OpenGL flip of rendered textures lives
  there. **[done]**
- A backend gives only `createDevice(api, window)` and
  `createUiIntegration(api, window, device)` (`render/GraphicsBackend.hpp`).
  `Renderer` and `RenderTarget` are engine classes over them;
  `createRenderer(api, window)` puts them together, so the editor and
  `Application` did not change. A renderer can be made over the test RHI
  without an interface. **[done]**

### Contracts

- **Lifetime of GPU objects.** RHI objects are abstract classes in
  `unique_ptr`; whoever created one owns it. Destroying an object does not
  free it at once: the device frees it when the frames that may still use
  it are finished (in OpenGL that is immediately, in Vulkan and D3D12 after
  the frames in flight). Commands refer to objects by pointer, valid until
  the end of the frame. GPU copies of resources are keyed by resource ID and
  revision (`GpuResourceCache`), never by the address of CPU data: a new
  revision replaces the copy, the old one is freed by the same rule.
- **Resource states.** A texture is in one state at a time: colour target,
  depth target, shader read, present. A change is an explicit command
  (`transition`); the graph inserts them between passes. OpenGL ignores
  them, Vulkan and D3D12 turn them into barriers. Only these four states for
  now.
- **Pipelines** are immutable after creation: shaders, vertex layout, depth
  test and write, face culling, blending, formats of the targets, sample
  count. A pipeline draws only into a target of the same formats and sample
  count; a mismatch is an error (checked by the test RHI and in debug
  builds). The window back buffer of every device has
  `rhi::kBackBufferColorFormat` and `rhi::kBackBufferDepthFormat`; the frame
  formats are in `render/FrameFormats.hpp`: `kHdrColorFormat` for the views,
  `kOutputColorFormat` and `kDepthFormat` for the final target (the window
  formats, so one pipeline draws into the window and a `RenderTarget`).
- **`Renderer`** owns the device and the frame: present, size, vsync, the
  interface and its texture previews. What a scene frame draws and in which
  order is `SceneRenderer`. A GPU sampler from the import settings of a
  texture is `samplerDesc` (`render/TextureSampler.hpp`), shared by the
  previews and the materials.
- **Shader data** goes through uniform blocks by frequency: frame (time,
  lights), view (camera), material (step 5), object (matrices). The layout
  is std140; the C++ structs repeat it with explicit `alignas` and are
  checked against the reflection at shader load (size and offsets of every
  field), not trusted to match. Per-object data lives in one buffer per
  frame, bound with an offset aligned to the device limit
  (`GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT` in OpenGL).
- **One convention for clip space** across the engine: right-handed world,
  clip depth from 0 (near) to 1 (far), NDC y up, texture rows from the top.
  Projections are built only this way (`math::perspective`), so cameras do
  not know the API; there is no `DepthRange` any more. The backend adapts: OpenGL gets its
  vertex shaders from SPIRV-Cross with the depth remapped to -1..1
  (`fixup_clipspace`), Vulkan will flip y with the viewport. The vertical
  flip of render targets in OpenGL stays inside the backend (`UiTexture`
  corners).
- **Textures**: 2D or cube (`TextureDesc::cube`, six faces +X, -X, +Y,
  -Y, +Z, -Z), with or without a full mip chain. A pass draws into one
  face and one mip of its colour target (`RenderPassDesc::colorFace`,
  `colorMip`, the viewport is that mip); `generateMipmaps` fills the mips
  from mip 0 outside a pass (in the graph: a pass with `generateMipmaps`).
  A sampler with mipmaps reads mips only if the texture has them: a texture
  without mips is complete for any sampler (OpenGL: `GL_TEXTURE_MAX_LEVEL`).
  Cube faces are sampled seamlessly across edges, as in every API.
- **Lost device**: the path that changes the graphics API without a restart
  (see below) also recovers from a lost device in Vulkan and D3D12. OpenGL
  does not lose its device, so nothing is added now.

### Shaders

Written once in GLSL 450 in the Vulkan style (`set`, `binding`), compiled by
glslang to SPIR-V. SPIR-V is the intermediate form: OpenGL gets GLSL 330
from SPIRV-Cross, Vulkan will take SPIR-V as it is, D3D12 HLSL from
SPIRV-Cross. Portability is not automatic: the generated code and the
profile below are checked on every target API when its backend arrives.

- **Shader profile**, what every target API can do; a shader outside it is
  rejected at compile time with a message:
  - stages: vertex and fragment only;
  - resources: std140 uniform blocks and combined samplers (`sampler2D`,
    `samplerCube`); no storage buffers, images or separate samplers;
  - sets: 0 frame, 1 view, 2 material, 3 object;
  - vertex inputs by fixed location: 0 position, 1 normal, 2 `uv0`,
    3 tangent (`vec4`, `w` is the bitangent sign);
  - limits of OpenGL 3.3: at most 16 textures per shader, a uniform block
    at most 16 KB.

  The profile has a version; a shader file of step 5 records it in its
  `.meta`, and a new profile version is a migration (see
  [project formats](project.md)).
- **Reflection gives structure, not presentation.** `ShaderReflection` reads
  from SPIR-V the uniform blocks, field names, types, offsets and sizes,
  sets and bindings, textures. How a field is shown in the material
  inspector (a 0..1 slider for `roughness`, a colour picker for `baseColor`,
  a label, a group) comes from annotations next to the field in the same
  shader file and maps onto the property attributes of
  [reflection](reflection.md). Types and bindings are never written twice;
  the annotation syntax is chosen in step 5.
- **Compiling**: `#include` (relative to the file and from the engine
  shader library), defines and compile options, errors with the source file
  and line, results cached in memory by source, includes and defines for
  the current run.
- **The device takes SPIR-V**, not GLSL source. The compiler (glslang) is a
  separate part used by the editor and the engine at start-up; the game
  built without the editor can ship precompiled shaders and run without
  glslang (step 9). SPIRV-Cross stays in the OpenGL backend.
- **Only what a stage uses** gets into the reflection: a block that is
  included but never read is not a resource of the shader and does not
  have to be set before a draw. So a shader may include the whole engine
  library (`<yqlora/uniforms.glsl>`) and keep only what it reads.
- **Built-in shaders** **[done]** live in `engine/shaders` and are embedded
  into the engine library. Their path in the compiler starts with
  `engine:` (`engine:standard.yshader`); errors show the same path.
  `#include <yqlora/...>` takes a file from `engine/shaders/include`, so the
  shaders of a project in step 5 include the same library.
- **Where it lives** **[done]**: `engine/internal/yqlora/shader` -
  `ShaderCompiler` (glslang, includes, defines, cache), `reflectShader` and
  `checkShaderProfile`, `UniformLayout` with `checkUniformLayout` (the C++
  side of a block: `YQLORA_UNIFORM_MEMBER` gives the name, offset and
  size of a field, `YQLORA_UNIFORM_STRUCT_MEMBER` also the layout of a
  struct field or of the element of an array of structs, whose fields are
  checked the same way), `buildShader` (the whole path, errors of every step
  together). The OpenGL backend has `GlslTranslator`: SPIR-V to GLSL 330,
  resources and varyings renamed so that GLSL 330 finds them by name.

### Prepare and the render graph

- **Prepare** works per view: one object can be seen in the Game window and
  not in the Scene window. Culling by the bounds of the mesh (AABB, carried
  by `DrawItem`) against the frustum and by the layer mask; opaque items
  sorted front to back with grouping by pipeline and material, transparent
  ones back to front against that view. **[done]** without the grouping and
  the transparent list: there is one pipeline and nothing transparent yet,
  both come with materials in step 5. `prepareView` (`render/Prepare`)
  takes the world bounds that Extract puts into `DrawItem`; the sort is
  stable, so equal distances keep the frame order.
- **The graph, MVP scope**: a pass declares the textures it reads and
  writes; the graph finds the dependencies, orders the passes, creates and
  frees the transient textures (the outline mask) within the frame, inserts
  the state transitions, and reports a cycle or a read of a texture nobody
  wrote. Not now: memory aliasing, async queues, parallel recording,
  automatic barrier analysis beyond the four states. They come with Vulkan
  and D3D12 when needed. **[done]**: `RenderGraph` lives for one
  `Renderer::render` call. A reader goes after every pass that writes its
  texture, passes into one texture keep the order they were added in (views
  draw over each other). Imported textures (back buffer, render target)
  count as written and end in the state asked for; transient ones come from
  `TransientTextures`, reused by the next frame and freed after 60 unused
  frames.
- **Editor passes**: a view takes a list of additional passes from its
  owner. The Viewport adds the grid and the selection outline to its own
  view; the passes live in the editor and keep their own data (selected
  nodes, colour), the engine only runs them. `FrameData::editor` goes away,
  the engine knows nothing about the editor. **[done]**: `ViewData::passes`
  holds `ViewPass` objects; each adds its graph passes after the scene of
  the view and gets what the view sees (`ViewPassInput`). The passes are
  made over `Renderer::context()` (device, shader compiler, uniforms,
  meshes) and are dropped with the renderer. The grid and outline live in
  `editor/render` with their shaders in `editor/shaders`, embedded into the
  editor library (paths `editor/...`); the outline mask is a transient
  texture of the graph, and only visible selected items are outlined.

### Phases (a commit per phase)

1. **R.1 RHI and OpenGL** **[done]**. `rhi::Device`, `rhi::CommandList`, buffers,
   textures, samplers, pipelines, render targets, resource states, the
   lifetime rule. The OpenGL implementation: `GlMesh`, `GlTexture`,
   `GlRenderTarget` become RHI objects, the old passes work over them. A
   minimal test RHI that records commands and checks pipeline and target
   compatibility, from the first phase, so that mistakes of the interface
   are found before the OpenGL implementation fixes them.
2. **R.2 Shaders** **[done]**. glslang and SPIRV-Cross, the profile, `ShaderReflection`,
   includes, defines, errors with file and line, the in-memory cache, the
   checks of std140 structs against the reflection; built-in shaders in GLSL
   450; the clip space convention, `DepthRange` leaves the cameras.
3. **R.3 The renderer in the engine** **[done]**. `Renderer` is an engine class over
   `rhi::Device`, `MeshPass` and `GpuResourceCache` move to `engine/render`,
   `createDevice(api)`, ImGui as a separate integration of the backend. The
   editor and `Application` change only where they create the renderer.
4. **R.4 Prepare and the graph** **[done]**. Culling, sorting, bounds in `DrawItem`, the
   graph in the MVP scope, the grid and outline passes in the editor,
   `FrameData::editor` removed.
5. **R.5 Tests and documents** **[done]**. Tests on the test RHI: pass order and graph
   dependencies, culling, sorting, no extra draws, the textures each pass
   uses. Small OpenGL integration tests (buffers, textures, pipelines, a
   frame into a target) that run where an OpenGL context can be created and
   are skipped otherwise. The marks `[planned, RHI]` in this file and in
   [frame](frame.md) are removed. The OpenGL tests (`tests/test_opengl.cpp`)
   open a hidden window and read the target back with
   `rhi::Device::readPixels` (rows from the top, for tests and screenshots):
   an uploaded texture sampled the right way up, a frame with a mesh, the
   grid and the outline, the depth test with clip depth 0..1.

Out of the stage: the Vulkan and D3D12 backends, the render thread and
post-processing (its place in the graph is ready); the sky and shadows are in
[step 5](materials.md#environment-planned-step-5).

## Meshes and textures [done]

- **A vertex** (`Vertex`): position, normal, `uv0`, tangent (`vec4`, like
  glTF `TANGENT`). `uv0 = (0, 0)` is the top left corner of the picture, as
  in glTF.
- **A mesh** (`MeshData`): vertices, indices of triangles counter-clockwise
  from outside, submeshes and bounds (AABB). A submesh is a range of indices
  with its own material; its index is the material slot of a `MeshRenderer`
  (step 5), and it keeps the index of the material of the source file. The
  bounds speed up picking, later culling.
- **A texture** (`TextureData`): RGBA one byte per channel, rows from the
  top, and how to read it: sRGB or Linear, filter, wrap, mipmaps (import
  settings). Textures go onto meshes through materials, step 5.
- **GPU copies** are kept by the backend itself, by resource ID and revision
  (`GpuResourceCache`): a `DrawItem` carries the mesh with its ID and
  revision, a copy of another revision is replaced, and a copy not drawn for
  120 frames is released. One template for every backend and kind of
  resource.
- **A texture in the interface**: `Renderer::uiTexture` gives the texture of
  a resource for ImGui (the preview in the inspector) with the colours of
  the file, without sRGB decoding: ImGui draws into a plain buffer, and a
  decoded picture would be darker.

## Cameras and rendering [done]

Prepared for several cameras (for example the player's arms drawn by a second
camera over the world, so they never go into walls); the arms themselves are
not part of this.

- `Camera3D` has an **order** (lower is drawn first), a **culling mask** of
  layers, a **clear mode** (colour and depth, depth only, nothing) and a
  **background** colour for the first mode. The background is the camera's,
  not the scene's: a second camera clearing colour and depth uses its own.
- `RenderSystem` keeps the cameras of the scene (a camera adds itself when
  it enters the scene) and gives those with an active node, sorted by order. The frame data carries a list of views instead of one camera; the
  backend draws them in turn, each with its own clear and only the items whose
  layer is in its mask.
- The **main camera** is the first one in that order, the one the game is
  seen through: `scene.system<RenderSystem>()->mainCamera()`. It is not
  stored anywhere, neither in `Scene` nor in the scene file; to change it,
  change the order or deactivate a camera. A camera that leaves the scene
  drops out of the list by itself.
- **Choosing the main camera in the editor**: the inspector of a camera says
  "Main camera" for the main one and "Inactive" for a camera whose node is
  off; the others get a "Make main" button. It sets the camera's order one
  below the main camera's (`orderToBecomeMain`), one undoable step. There is
  deliberately no "main" flag next to the order: two sources would contradict
  each other (two flagged cameras, none flagged), and the order still decides
  which camera draws over which **[done]**.
- The editor's own camera (no game camera, or not playing) is drawn as one
  view with the background from the editor theme.
- **Camera preview** in the editor: while a camera is selected, a
  small window in the corner of the viewport shows the picture through it, so
  the camera can be placed by eye. A button on the viewport toggles it, the
  choice is an editor setting (`EditorSettings`, a file in the user folder)
  **[done]**.
- **Two windows during Play**: "Game" shows the main camera,
  "Scene" keeps the free editor camera, where nodes are selected and changed
  while the game runs. Changes made during Play belong to the Play copy and
  are gone after Stop **[done]**. The Game window comes forward and takes the
  input on Play; without a game it shows the edited scene through its main
  camera. Mouse position relative to the Game window comes with the HUD
  **[planned, step 8]**.
- A scene switch requested by a script happens between frames, never in the
  middle of an update **[planned, step 9]**.

## Changing the graphics API [done]

The API comes from the Graphics section of the project settings (one per
system) and changes without restarting the application. `Application`
compares it with the running renderer every frame; on a change it rebuilds
only the render part:

```
onRendererLost      owners release what they created through the renderer
                    (the editor: textures of the scene windows)
UiLayer             drops the ImGui drawing of the old API; the ImGui
                    context, windows and layout stay
Renderer            destroyed while its context is alive
Window              recreated only if the new API needs other window flags
                    (OpenGL and Vulkan windows differ in SDL)
Renderer            created for the new API
UiLayer             ImGui drawing for the new API
```

- The scene, the project, the edit history and the editor state live outside
  the renderer, which sees only the frame snapshot, so nothing else notices.
- An API that is not built into this engine, or that the system refuses,
  leaves the current one in place with a message in the log, once.
- Vsync is switched on the running renderer, without a rebuild.
- The same path will recover from a lost device (Vulkan, D3D12), see
  [the RHI stage](#the-rhi-stage-done) **[planned, Vulkan, D3D12]**.
