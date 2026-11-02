# Shaders, materials and lights

[Русская версия](materials.ru.md)

Step 5 of the MVP: shaders written by the project, materials that give a
shader its values and textures, and light sources in the scene. Built on the
[RHI stage](rendering.md#the-rhi-stage-done): shaders go through the same
compiler, profile and reflection as the built-in ones.

Status marks: see [README](README.md).

## Decisions [planned, step 5]

- **Three parts, each on its own** (as the [roadmap](../../README.md#roadmap)
  says): a shader is a resource, a material is a resource that names a
  shader, a light is a node. A material knows nothing about lights, a light
  knows nothing about materials; they meet only in the shader.
- **Lighting is PBR metallic-roughness**, as in glTF and the Standard shader
  of Unity: base colour, metallic, roughness, normal map, emission. glTF
  materials come in without loss.
- **Colour lives in the material.** `MeshRenderer` loses its colour; an
  upgrade of the project turns every distinct colour into a material file
  (see "Upgrading old scenes").
- **Material edits go straight into the file**, as in Unity, without
  pressing "Save"; the edit history of the scene does not hold them. Undo
  for materials can come later; assigning a material in the scene is a
  scene edit and is undone as one.
- **Forward rendering, every light per pixel**, no shadows: shadows, sky and
  post-processing come after the MVP (their place in the graph is ready).
- **Linear colour, one output pass.** Lighting and blending of transparent
  surfaces happen in linear RGB:
  1. colour textures (base colour, emission) are sampled as sRGB and
     decoded by the GPU (`Rgba8Srgb`); data textures (normal, metallic,
     roughness) are linear;
  2. the views of a frame are drawn into a linear HDR colour target
     (`Rgba16Float`, a transient texture of the graph) with the depth of the
     final target, so transparent items blend in linear RGB;
  3. an output pass (`<yqlora/output.glsl>`) encodes the HDR target into
     sRGB and writes the final target (the window or a `RenderTarget`,
     `Rgba8`), which ImGui shows unchanged. Tone mapping takes the same
     pass after the MVP.

  Passes of a view's owner (the grid, the outline) draw over the final
  target after the output pass, with colours given in sRGB, and still test
  the depth of the scene. Writing into `Rgba8Srgb` targets
  (`GL_FRAMEBUFFER_SRGB`) is not used: ImGui would decode such a texture
  when showing it, and OpenGL 3.3 has no core way to skip that.

## Shader files [planned, step 5]

One file `.yshader` in GLSL 450 holds both stages. The engine compiles it
twice, with `YQLORA_VERTEX` and with `YQLORA_FRAGMENT` defined, through the
same `ShaderCompiler`, profile and reflection as the built-in shaders.

```glsl
#version 450
#include <yqlora/standard.glsl>

layout(set = 2, binding = 0, std140) uniform Material {
    vec4 baseColor;   //@ color default(1, 1, 1, 1)
    float roughness;  //@ range(0, 1) default(0.5)
} material;

layout(set = 2, binding = 1) uniform sampler2D baseColorMap; //@ default(white)

#ifdef YQLORA_VERTEX
void main() { standardVertex(); }
#endif

#ifdef YQLORA_FRAGMENT
void main()
{
    Surface surface = defaultSurface();
    surface.baseColor = material.baseColor.rgb * texture(baseColorMap, vUv0).rgb;
    surface.roughness = material.roughness;
    fragColor = shade(surface);
}
#endif
```

- **The engine library** (`#include <yqlora/...>`): `uniforms.glsl` (frame
  with lights, view, object), `vertex.glsl` (inputs by location),
  `lighting.glsl` (the PBR functions, `Surface`, `shade`), `output.glsl`
  (sRGB encoding) and `standard.glsl` (all of them plus `standardVertex`
  and the varyings it fills). A project shader may include its own files
  next to it.
- **Material block**: set 2, binding 0, named `Material`; textures of the
  material: set 2, bindings from 1. Types and bindings come only from the
  reflection, never written twice.
- **Annotations** for the material inspector. Grammar: a line comment that
  starts with `//@`, then annotations separated by spaces; an annotation is
  a word with optional arguments in brackets (numbers, or one string in
  quotes). Annotations on their own lines apply to the next declaration in
  the `Material` block or the next texture of set 2; a trailing `//@` on the
  line of a declaration applies to it too. A `//@` line not followed by such
  a declaration is an error.

  ```glsl
  //@ label("Roughness") range(0, 1)
  float roughness;  //@ default(0.5)
  ```

  - `color`: only `vec3` and `vec4`; a colour picker, the value is sRGB and
    is converted to linear for the shader (alpha stays as is);
  - `range(min, max)`: only `float`, `int`, `uint`; a slider, `min < max`;
  - `default(...)`: as many numbers as the type has components; textures
    take `white`, `black` or `normal`;
  - `label("text")`, `hidden`: any field or texture;
  - an unknown word, a wrong number or type of arguments, an annotation not
    fitting the type, the same annotation twice: a compile error with the
    file and line.
- **The profile** gets the tangent input: location 3, `vec4`, as glTF
  `TANGENT`: `xyz` the tangent, `w` the sign of the bitangent, so mirrored
  UV layouts work (`bitangent = cross(normal, tangent.xyz) * tangent.w`).
  `Vertex` becomes position, normal, `uv0`, tangent. The profile stays at
  version 1: no project has shaders yet. A shader file records the profile
  version in its `.meta`.
- **Built-in shaders as resources**: `Standard` (the PBR shader every new
  material uses) and `Error` (magenta, for a material whose shader does not
  compile) have constant IDs below `kFirstProjectId`, like the built-in
  meshes.
- **Recompiling**: when the editor window gets focus, changed `.yshader`
  files and files they include are compiled again (the compiler cache knows
  the includes). A new revision rebuilds the pipelines of the materials that
  use the shader. Errors go to the log panel as `file:line: text`; a
  material whose shader has an error draws with `Error` until it compiles.

## Materials [planned, step 5]

A material file `.ymat` (JSON, `FileFormat` version 1):

```json
{
    "shader": "0000000000000101",
    "blend": "opaque",
    "cull": "back",
    "parameters": {"baseColor": [1.0, 0.5, 0.2, 1.0], "roughness": 0.4},
    "textures": {"baseColorMap": "4c1e07a95b3d2f60"},
    "version": 1
}
```

- Values are kept **by the name of the field**:
  - in the shader and in the material: the value of the material;
  - only in the shader: its `default`;
  - only in the material: kept in the file, not sent to the GPU;
  - back in the shader later: the kept value is used again.

  Saving a material never drops values the shader does not have. A value
  whose type differs from the field (a `vec4` kept for a field that became
  `float`) is not used and the field takes its `default`; the value stays
  in the file.
- **Render state of the material**: `blend` and `cull` (back or none).
  `opaque`: depth test and write, no blending. `transparent`: alpha
  blending in linear RGB, depth test without depth write, drawn after the
  opaque items. Pipelines are cached by shader revision, render state and
  target formats.
- **Writing** a `.ymat` is atomic: a temporary file next to it, then a
  replace (`platform::writeFileAtomically`), so a crash while saving does
  not break the material.
- **On the GPU** a material is a uniform buffer written by the offsets of
  the reflection, plus its textures and samplers, keyed by material ID and
  revision and by shader revision (`GpuResourceCache`).
- **Slots**: `MeshRenderer::materials` is a list of material references,
  one per submesh. An empty or missing slot draws with the built-in default
  material (`Standard`, white). The inspector names a slot by the material
  of the source file (glTF) or by its number.
- **Prepare** groups opaque items by pipeline and material (front to back
  inside a group) and draws transparent items after them, back to front.

## Materials from scripts [planned, step 5]

A script changes how one object looks without touching the material file,
as `MaterialPropertyBlock` in Unity does:

```cpp
auto& renderer = node().component<MeshRenderer>();
renderer.overrides(0).setColor("baseColor", {1.0f, 0.2f, 0.2f, 1.0f});
renderer.overrides(0).setFloat("roughness", 0.1f);
renderer.overrides(0).setTexture("baseColorMap", hitTexture);
renderer.overrides(0).clear();
```

- `MaterialOverrides` holds values and textures by field name for one slot
  of one `MeshRenderer`. They belong to that object only: the shared
  material (its file, its data in the cache, its `GpuMaterial`) never
  changes, so another object with the same material stays as it was.
- `setFloat`, `setVector`, `setColor`, `setTexture` replace the value of
  one field; `clear(name)` removes that override and the field takes the
  value of the material again; `clearAll()` removes all of them. Overrides
  are not saved with the scene.
- An override keeps its type. When the item is drawn, overrides are matched
  against the layout of the current shader by name and type: a name the
  shader does not have, or a field of another type (after a shader reload or
  a new material in the slot), is skipped with one message in the log and
  never written into another field.
- Scripts see only names and typed values, never `GpuMaterial` or the
  reflection.
- An item with overrides gets its own material data in the frame buffer of
  uniforms (the material values with the overrides on top, the same
  layout), so overrides cost one small write per object per frame and no
  new GPU objects.
- Lights are nodes, so a script changes them through their properties
  (`light.setIntensity(...)`), with no new API.
- A material file can also be changed from a script through the resource
  (`materialData.set...`) for all objects at once; such a change is not
  written to disk.

## Lights [planned, step 5]

- **Nodes** (engine nodes, see [scene](scene.md)): `DirectionalLight3D`,
  `PointLight3D`, `SpotLight3D`, all `Node3D`. Common: colour (sRGB, shown
  as a colour), intensity. Point and spot: range. Spot: inner and outer
  angle. The direction is the `-Z` of the node, like a camera.
- **Extract** puts the lights of the scene into `FrameData::lights` (world
  position and direction, linear colour times intensity); a disabled node
  gives no light.
- **The frame block** holds the ambient colour, up to 4 directional lights
  and up to 32 point and spot lights together, with their counts. More
  lights than that: the nearest to the camera of the first view are kept
  and the log says so once. A near light may still miss an object; per
  object light lists come later.
- **Falloff** of point and spot lights is inverse square with a smooth
  window to zero at the range, as in glTF `KHR_lights_punctual`:
  `intensity / max(d², ε) * saturate(1 - (d / range)⁴)²`; a spot also fades
  smoothly between the inner and outer angle.
- **Ambient light**: a colour and intensity in the Lighting section of the
  project settings, until environment settings per scene appear. It is a
  separate constant term (diffuse only), not image-based lighting; it does
  not replace direct lights, and environment maps and IBL are outside step
  5.
- **A new scene** has a camera and a directional light, as in Unity and
  Godot, so a new object is lit at once.
- **The editor** draws light icons and their range or direction in the
  viewport, as it does for cameras.

## Upgrading old scenes [planned, step 5]

The scene format goes to version 2: `MeshRenderer` has `materials` instead
of `color`. A document migration cannot create files, so the upgrade of a
project gets a project step before the documents: it reads every scene,
collects the colours of `MeshRenderer`, writes one material file per
distinct colour (`materials/Color_RRGGBBAA.ymat`, the `Standard` shader
with that base colour, opaque, or transparent if alpha is below 1) and gives
the scene migration the colour to material map. Colours are compared after
rounding every channel to 8 bits (`round(clamp(c, 0, 1) * 255)`), so two
colours that differ less than that share one material; the material keeps
the rounded value. A colour equal to the default material (white) gets no
file. A file with that name already in the project is reused if it holds
the same colour, otherwise the name gets a number. Everything is prepared in memory first and written
only if every scene was read, as `upgradeProject` already does.

## Editor [planned, step 5]

- "Create resource" in Files gets **Shader** (from the template
  `assets/templates/shader`, a lit shader with a colour, a texture,
  metallic and roughness) and **Material** (`Standard`, white), through the
  window with a name and "Create".
- **The material inspector**: a preview sphere, the shader field, the
  render state, then the fields of the `Material` block in the order of the
  shader with their annotations, then the textures as resource fields of
  textures. A change reaches the material in the cache at once (new
  revision, every object and the preview update while a slider moves); the
  file is written when the edit ends (the widget is released) and at most
  twice a second during a long drag, atomically.
- **The preview** has its own render target, camera, sphere and fixed
  light; nothing of it enters the scene, and its light does not light the
  scene.
- **The shader inspector**: the result of the last compile (fine, or the
  errors), the material block and the textures it declares; "Open" opens the
  file in the system editor. Editing shaders inside the editor comes with
  the Scripts tab (see [code editor](code-editor.md)).
- **The `MeshRenderer` inspector** shows the slots by name; a material is
  dropped from Files onto a slot, or onto an object in the viewport (the
  slot of the submesh under the cursor).

## glTF materials [planned, step 5]

- Materials and images of a model become nested resources of the file
  (`material:Name`, `texture:Name`), with IDs in `.meta` as meshes have.
  Images inside a GLB are read from its buffer.
- A material of a model is read-only: its values come from the file. To
  change it, the inspector offers "Make a copy", which writes a `.ymat`
  next to the model.
- Dropping a model into the scene fills the slots of every `MeshRenderer`
  with the materials of the file.
- Tangents come from `TANGENT` of the file with their `w`; when a material
  of the mesh has a normal map and the file has none, they are computed per
  vertex from positions, `uv0` and the indices (MikkTSpace-like: summed per
  triangle, orthogonal to the normal, `w` from the UV orientation).
- Nested materials and textures keep their IDs in `.meta` like meshes: a
  re-export keeps the references of the scenes.

## Phases of step 5 (a commit per phase) [planned, step 5]

### 5.1 Lights and PBR

- **Light nodes** in `engine/include/yqlora/render/`: `Light3D` (colour,
  intensity) and `DirectionalLight3D`, `PointLight3D` (range), `SpotLight3D`
  (range, inner and outer angle), registered in `registerRenderTypes`, with
  icons in the tree and in "Add node".
- **`RenderSystem`** keeps the active lights of the scene, as it keeps the
  cameras (enter and exit of the scene, active and enabled).
- **`FrameData::lights`** and `FrameData::ambient`: Extract writes world
  position and direction, linear colour times intensity, range, cosines of
  the spot angles. Limits: 4 directional, 32 point and spot; with more, the
  nearest to the camera of the first view are kept and the log says so once.
- **Lighting section** of the project settings (`LightingSettings`): ambient
  colour and intensity.
- **The frame block** (`FrameUniforms`) gets the ambient light, the light
  arrays and their counts; `checkUniformLayout` compares the size and the
  offset of every field, and for arrays also the element stride.
- **HDR target and output pass**: `rhi::Format::Rgba16Float`; the scene of
  every view goes into a transient HDR texture, `OutputPass` encodes it to
  sRGB into the final target, the passes of a view's owner come after it.
- **One file, two stages**: `buildShaderFile` compiles one file with
  `YQLORA_VERTEX` and then `YQLORA_FRAGMENT`; errors keep the line of the
  file.
- **Shader library** in `engine/shaders/include/yqlora/`: `lighting.glsl`
  (`Surface`, `defaultSurface`, `shade`: GGX specular, Smith geometry,
  Schlick Fresnel, Lambert diffuse, point and spot falloff by range),
  `output.glsl` (linear to sRGB), `standard.glsl` (both plus
  `standardVertex` and the varyings `vWorldPosition`, `vNormal`, `vUv0`).
- **The built-in `Standard` shader** (`engine/shaders/standard.yshader`)
  replaces `mesh.vert` and `mesh.frag`. It needs position, normal and
  `uv0`; the tangent joins in phase 2. Until phase 3 `MeshPass` fills its
  material block from the colour of `MeshRenderer` (metallic 0, roughness
  0.5).
- **A new scene** gets a camera and a `DirectionalLight3D` turned down and
  to the side, through the same `ProjectFiles::createScene` that makes new
  scenes now.
- **Editor** (in `editor/ui`, not in the renderer): `LightGizmo` draws the icon of every light and, for the
  selected one, its direction, range sphere or cone.
- **Tests**: lights in the frame (disabled ones skipped, limits, the
  nearest kept); the frame block matches its struct; one-file build with
  errors on the right line; on OpenGL: the side of a cube facing a
  directional light is brighter than the opposite side, a point light out
  of range adds nothing, a half transparent white quad over black gives the
  linear middle (sRGB about 188), not 128.

### 5.2 Shader resources

- **Annotations** (`engine/shader/ShaderAnnotations`): the `//@` grammar
  of "Shader files", bound to the next declaration or to the declaration of
  the same line, read into property attributes (`color`, `range`,
  `default`, `label`, `hidden`) and checked against the type from the
  reflection; every error has the file and line.
- **`ShaderData`** (resource kind `shader`): the device description
  (SPIR-V and reflection), the material layout (fields with type, offset,
  size, attributes, default), the textures (name, binding, default), the
  files it includes, the errors of the last compile.
- **`ShaderLoader`** for `.yshader`: compiles through the project reader
  (includes next to the file, `<yqlora/...>` from the engine), with the
  profile; a failed compile is the resource error state with all messages.
- **Dependencies of a resource**: `LoadResult` lists the files read besides
  the resource file; reloading on focus also looks at them, so a changed
  include recompiles the shaders that use it.
- **Tangents in the profile**: location 3, `vec4`; `Vertex` gets
  `tangent`, `meshVertexLayout` passes it, glTF `TANGENT` is read when
  present (computing them is phase 5).
- **`.meta` of a shader** records the profile version.
- **Built-in resources** `Standard` and `Error` (`builtin::kStandardShader`,
  `builtin::kErrorShader`), loaded from `engine:` like the built-in meshes.
- **Errors in the log** as `file:line: text`, one block per compile, in the
  log panel next to the script build errors.
- **"Create resource > Shader"** from `assets/templates/shader` through the
  window with a name and "Create".
- **Tests**: annotations parsed on their own lines and trailing; unknown
  ones, wrong arguments and ones not fitting the type rejected with the
  line; the loader compiles both stages of one file; an error inside an
  include names that file and line; a changed include reloads the shader;
  the template compiles; the `.meta` holds the profile version.

### 5.3 Materials

- **`MaterialData`** (resource kind `material`, format `material` version
  1): shader, `blend`, `cull`, values and textures by name; loader and
  writer of `.ymat` (atomic, `platform::writeFileAtomically`); values the
  shader lacks are kept.
- **Built-in materials**: the default one (`Standard`, white) and `Error`
  (the `Error` shader); a material whose shader is broken or missing draws
  with `Error`.
- **GPU materials** (`engine/render/GpuMaterial`): the uniform buffer
  written by the offsets of the material layout (defaults, `color` values
  from sRGB to linear), textures with samplers from their import settings,
  default textures `white`, `black`, `normal`; keyed by material and shader
  ID and revision.
- **sRGB textures**: a texture imported as sRGB becomes `Rgba8Srgb` on the
  GPU, so sampling decodes it.
- **Pipeline cache** (`engine/render/PipelineCache`): by shader and its
  revision, blend, cull, target formats; old revisions are released with the
  GPU cache rule.
- **Slots**: `MeshRenderer::materials`, a list of material references; the
  colour goes. Extract makes one `DrawItem` per submesh with its index range
  and material; `drawMesh` draws a range.
- **Overrides for scripts**: `MeshRenderer::overrides(slot)` with
  `MaterialOverrides` (see "Materials from scripts"); `DrawItem` carries
  them, the pass writes the material data of such an item per frame.
- **Prepare**: opaque items grouped by pipeline and material, front to back
  inside a group; a transparent list, back to front, drawn after the opaque
  ones in the same view.
- **Scene format 2** and the project step of the upgrade (see "Upgrading
  old scenes"): `upgradeProject` gets steps over the whole project that run
  before the document migrations.
- **Tests**: values by name and defaults; a value the shader lost is kept
  and written back, and used again when the field returns; a value of
  another type is not used; the pipeline cache gives one pipeline per
  state; grouping and transparent order in Prepare; overrides change one of
  two objects with the same material, `clear` gives back the value of the
  material, an override of the wrong type is skipped, overrides are not
  saved; colours that round to the same RGBA8 share one material; upgrading a project with coloured objects writes
  materials and scenes; a broken shader draws with `Error` and comes back
  after a fix; on OpenGL: a texture of a material is sampled as sRGB, an
  override changes the colour of one of two cubes.

### 5.4 Materials in the editor

- **"Create resource > Material"**: `Standard`, white, the window with a
  name.
- **Material inspector**: preview, shader field (shaders only), blend and
  cull, the fields of the material block with the widgets of their
  attributes, the texture fields (textures only, with preview). A change
  updates the material in the cache at once, so every object using it and
  the preview change while a slider moves; the file is written atomically
  when the edit ends and at most twice a second during a long drag.
- **Preview**: a sphere with the material, lit by one fixed directional
  light and ambient, in its own render target at the top of the inspector;
  drawn while the inspector shows it; its camera, sphere and light are not
  in the scene.
- **Shader inspector**: compiled or the errors (a click opens the file at
  the line in the system editor), the fields and textures it declares,
  "Open".
- **`MeshRenderer` inspector**: one row per submesh with the slot name
  (the material of the source file, or "Slot N") and a material field.
- **Dropping a material** from Files onto a slot, or onto an object in the
  viewport: picking returns the submesh under the cursor and the slot is
  set in one undo step (a change of the scene, unlike edits of the
  material file).
- **Tests**: slot names; dropping onto an object sets the right slot and is
  undone; an edit in the inspector updates the material at once and writes
  the file when the edit ends, not on every frame of a drag.

### 5.5 glTF materials

- **Images of a model** become nested textures (`texture:Name`), from a
  file next to the model or from a buffer of the GLB, with the colour space
  taken from their use (base colour and emission sRGB, the rest linear).
- **Materials of a model** become nested materials (`material:Name`) of the
  `Standard` shader: base colour factor and texture, metallic and roughness
  factors and texture, normal texture and scale, emission, `alphaMode`
  (`BLEND`: transparent; `MASK`: opaque with the `alphaCutoff` field of
  `Standard`, which discards fragments below it, a real alpha test, not a
  silent substitution), `doubleSided` (cull none, back faces lit with the
  flipped normal).
- **Tangents** are computed per vertex when the material of a submesh has a
  normal texture and the file has none (summed per triangle along `uv0`,
  made orthogonal to the normal, the sign in `w`).
- **Dropping a model** fills the slots of every created `MeshRenderer` with
  the materials of the file, by the source material of each submesh.
- **"Make a copy"** in the inspector of a model material writes a `.ymat`
  next to the model with the same values and textures.
- **Tests**: a GLB with an embedded image and a material gives the nested
  texture and material; `TANGENT` keeps its `w`; computed tangents of a
  quad with a mirrored UV half have opposite `w`; `doubleSided` gives cull
  none; `MASK` gives `alphaCutoff`, not plain opaque; a re-export keeps the
  nested IDs; dropping fills the slots; the copy holds the same values.

### 5.6 Documents

This file, rendering (lighting, sRGB, Prepare with materials), resources
(shaders, materials, dependencies), scene (light nodes, `MeshRenderer`
slots and overrides), project (formats of `.yshader`, `.ymat`, scene 2,
project upgrade steps), project settings (Lighting), editor, the guide
"Your own shader and material", README. After this phase step 5 is closed.
