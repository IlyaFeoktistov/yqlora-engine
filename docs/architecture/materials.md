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
- **Lighting is PBR metallic-roughness**, as in glTF: base colour, metallic, roughness, normal map, emission. glTF
  materials come in without loss.
- **Maps of `Standard`** (each optional; a missing one is a default texture
  and the result is as without it):
  - `baseColorMap` (sRGB, alpha is opacity) times `baseColor`;
  - `ormMap`, a packed map as in glTF: R occlusion, G roughness,
    B metallic; factors `roughness`, `metallic`, `occlusionStrength`;
  - `normalMap` (tangent space, +Y up, as in glTF) with strength
    `normalScale`;
  - `heightMap`, a height map for parallax (parallax occlusion mapping:
    steps along the view ray in tangent space, the step count by angle),
    depth `heightScale`; at 0 parallax is off and costs nothing;
  - `emissionMap` (sRGB) times `emission` (a linear HDR colour, may exceed
    1, like `KHR_materials_emissive_strength`).
  Occlusion darkens only the ambient light, not direct lights (as in glTF).
  Separate metallic and roughness maps are packed into one on import
  later; detail maps, anisotropy, clear coat and subsurface scattering come
  after the MVP with photorealism.
- **Colour lives in the material.** `MeshRenderer` loses its colour. Old
  scenes are not upgraded: until the branch is released, file formats stay
  at version 1 without upgrades, and an object of an old scene draws with
  the default material (its `color` stays an unknown property).
- **Material edits go straight into the file**, without
  pressing "Save"; the edit history of the scene does not hold them. Undo
  for materials can come later; assigning a material in the scene is a
  scene edit and is undone as one.
- **Forward rendering, every light per pixel**, with shadows from the
  sun, spot and point lights (see "Shadows"); post-processing comes after
  the MVP (its place in the graph is ready).
- **Sky and sun**: the sky is a material with a sky shader
  (the built-in `Procedural` and `Panoramic` or your own), the sun is a
  directional light; a camera with the `Sky` clear draws the sky as its
  background, and the ambient light can come from the sky (see "Sky and
  environment").
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
     `kOutputColorFormat`), which ImGui shows unchanged. Tone mapping takes the same
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
  a word with optional arguments in brackets, separated by commas (numbers,
  words or a string in quotes). Annotations on their own lines apply to the next declaration in
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
  version in its `.meta` on import in the editor.
- **Built-in shaders as resources**: `Standard` (the PBR shader every new
  material uses) and `Error` (magenta, for a material whose shader does not
  compile) have constant IDs below `kFirstProjectId`, like the built-in
  meshes.
- **Recompiling**: when the editor window gets focus, changed `.yshader`
  files and files they include are compiled again (the compiler cache knows
  the includes). A new revision rebuilds the pipelines of the materials that
  use the shader. Errors go to the log panel as `file:line: text`; a
  material whose shader has an error draws with `Error` until it compiles.

## Materials [done]

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

## Materials from scripts [done]

A script changes how one object looks without touching the material file:

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
  written to disk (`SharedMaterial`: `setFloat`, `setVector`, `setColor`,
  `setTexture`; built-in materials do not change).

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
- **Ambient light**: in the Lighting section of the project settings,
  until environment settings per scene appear. It is a separate diffuse
  term, not image-based lighting: it does not replace direct lights, and
  sky reflections on metals and IBL are outside step 5. Phase 5.1 gives a
  constant colour and intensity, phase 5.5 adds the sky and a gradient (see
  "Sky and environment").
- **A new scene** has a camera and a directional light, so a new object is lit at once.
- **The editor** draws light icons and their range or direction in the
  viewport, as it does for cameras.

## Sky and environment [planned, step 5]

The sky is a material, the sun is a directional light, the
ambient light comes from the sky, a gradient or a colour.

- **A sky material** is an ordinary `.ymat` with a sky shader. A sky shader
  includes `<yqlora/sky.glsl>` and defines one function
  `vec3 sky(vec3 direction)`: the linear colour of the sky in a direction
  from the camera (world axes). The library gives everything else (`main`
  of both stages, a full screen triangle on the far plane, the direction of
  the pixel), so your own sky is just this function and a `Material` block
  with annotations, as for any material.
- **Built-in sky shaders** with fixed IDs, like `Standard`:
  - `Procedural`: the sun (none, simple,
    high quality), sun size, its convergence (edge softness), atmosphere
    thickness, sky tint, ground colour, exposure. The sky follows a
    simplified atmospheric scattering, so sunrise and sunset change its
    colour by themselves, from the height of the sun;
  - `Panoramic`: a panorama image (equirectangular, an sRGB texture), a
    rotation around the vertical, a tint, exposure.
  The built-in `Default Sky` material is `Procedural` with its defaults.
- **The sun** is the directional light with the `sun` flag; when no enabled light has the flag, the brightest directional
  light is the sun. The frame block gets `sunDirection`, `sunColor` and
  `hasSun`, so any shader (sky, water, your own material) knows where the
  sun is. The sun lights the scene as an ordinary directional light and
  stays in its array.
- **The `Sky` camera clear** (`CameraClear::Sky`): the sky pass draws the sky material of the settings after the
  opaque pass (depth test on the far plane, no write), so the sky fills
  only the pixels where nothing is. A new camera gets `Sky`.
- **The Lighting section** (`LightingSettings`): the sky material
  (`skybox`, only materials with a sky shader; empty: no sky, a `Sky`
  camera clears to black), the ambient source `ambientSource` (`Sky`,
  `Gradient`, `Color`), its multiplier `ambientIntensity`, the gradient
  colours (sky, equator, ground) and the colour for `Color`.
- **Ambient light from the sky** is second order spherical harmonics (9
  coefficients): the ambient pass calls `sky(direction)` of
  the same shader over a set of directions (a shader variant with the
  `YQLORA_SKY_AMBIENT` define) and writes the coefficients into a small
  texture. It runs only when the sky material, its shader or the sun
  changed, fully on the GPU with no read back to the CPU, so a day and
  night cycle does not stall the frame. `Gradient` and `Color` need no
  pass. `lighting.glsl` takes the ambient light along the normal
  (`ambientLight(normal)`) the same way for all three sources.
- **Editor**: the viewport has its own sky toggle; the
  material preview draws the sky behind the sphere; a sky shader shows the
  same fields in the material inspector as any other.
- Sky reflections on metals (environment maps, IBL), clouds and fog come
  after the MVP.

## Shadows [planned, step 5]

Lights give shadows, their meshes cast and receive them.

- **Lights**: every light has `shadows` (`None`, `Hard`, `Soft`),
  `shadowStrength` (0..1), `shadowBias` and
  `shadowNormalBias` (against shadow acne and peter panning). A new
  `DirectionalLight3D` and the sun of a new scene use `Soft`, point and
  spot lights use `None`.
- **Meshes**: `MeshRenderer::castShadows` and `receiveShadows`, both on by
  default.
- **A directional light** uses cascaded shadow maps (CSM): up to 4
  cascades in one depth texture (2 x 2), cascade splits by the practical
  scheme (a mix of uniform and logarithmic), each cascade snapped to the
  texels of the map so the shadow does not shimmer when the camera moves;
  a smooth blend between cascades. One directional light has shadows: the
  sun (see "Sky and environment"), and until phase 5.5 the first one with
  shadows.
- **A spot light** uses one perspective map, **a point light** six cube
  faces; both are tiles in a shared shadow atlas, up to 16 tiles per frame
  (a point light takes six). More shadowed lights: the nearest to the
  camera get shadows and the log says so once.
- **Softness**: `Hard` is one depth compared sample, `Soft` a PCF filter
  over a Poisson disc rotated per pixel, with no banding.
- **Passes**: shadows are drawn before the opaque pass, once per cascade
  and tile, depth only, with the depth shader of the mesh (a material with
  `alphaCutoff` gets its own after phase 5.3, otherwise a shared one). The
  frame block holds the cascade and tile matrices and the cascade splits;
  `lighting.glsl` dims the direct light of a light by its map, shadows do
  not dim the ambient light.
- **RHI**: a depth format the shader reads (`Depth32Float`) and a
  comparison sampler (`sampler2DShadow` in the shader profile).
- **Settings** (Lighting section): shadow distance (100 m by default), the
  cascade count, the size of a cascade map and of the atlas (quality).
- **After the MVP**: contact shadows, softness by light size (PCSS),
  shadows from transparent surfaces, ray tracing.

## Editor [done]

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

### 5.1 Lights and PBR [done]

- **Light nodes** in `engine/include/yqlora/render/`, a file per class:
  `Light3D` (a `Vec3` colour in sRGB, intensity; not created by itself)
  and `DirectionalLight3D`, `PointLight3D` (range), `SpotLight3D`, a point
  light in a cone, so it derives from `PointLight3D` (inner and outer angle
  from the axis); registered in `registerRenderTypes`, with icons in the
  tree and in "Add node". A `Vec3` colour property is a colour picker
  without alpha in the inspector.
- **`RenderSystem`** keeps the active lights of the scene, as it keeps the
  cameras (enter and exit of the scene, active and enabled).
- **`FrameData::lights`** and `FrameData::environment`: Extract writes world
  position and direction, linear colour times intensity, range, cosines of
  the spot angles. Limits: 4 directional, 32 point and spot; with more, the
  nearest to the camera of the first view are kept and the log says so once.
- **Lighting section** of the project settings (`LightingSettings`): ambient
  colour and intensity.
- **The frame block** (`FrameUniforms`) gets the ambient light, the light
  arrays and their counts; `checkUniformLayout` compares the size and the
  offset of every field, for arrays also the element stride, and the
  fields inside the light structs.
- **HDR target and output pass**: `rhi::Format::Rgba16Float`; the scenes
  of all views of a frame go in turn into one transient HDR texture with
  the depth of the final target (a later camera draws over an earlier one,
  as before), `OutputPass` encodes it to sRGB into the final target once,
  then the passes of the view owners follow. The camera background in
  `ViewData` is already linear.
- **One file, two stages**: `buildShader` gives every stage the
  `YQLORA_VERTEX` or `YQLORA_FRAGMENT` define, so both paths of the request
  are the same for a `.yshader` file; errors keep the line of the file.
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
- **A new scene** gets a root named after the scene, a camera and a
  `DirectionalLight3D` "Sun" turned down and to the side (`buildNewScene`
  in the editor); the starter scene of a new project begins with it too.
- **Editor** (in `editor/ui`, not in the renderer): `LightGizmo` draws the
  icon of every light and, for the selected one, its rays, range sphere or
  cone; projecting a point to the screen is shared by all gizmos
  (`GizmoDrawing`). A light is picked by a click on a sphere around it, as
  a camera is.
- **Tests**: lights in the frame (disabled ones skipped, limits, the
  nearest kept); the frame block matches its struct; one-file build with
  errors on the right line; on OpenGL: the side of a cube facing a
  directional light is brighter than the opposite side, a point light out
  of range adds nothing, a half transparent white quad over black gives the
  linear middle (sRGB about 188), not 128.

### 5.2 Shader resources [done]

- **Annotations**, a responsibility per file in `engine/shader/`:
  `ShaderAnnotations` parses the text (the `//@` grammar of "Shader
  files", bound to the next declaration or to the declaration of the same
  line; blank lines and ordinary comments may stand between them),
  `MaterialAnnotations` applies the annotations (`color`, `range`,
  `default`, `label`, `hidden`) to a field or texture by the type from the
  reflection. Every error has the file and line. Annotations are read only
  in the shader file itself, not in its includes: it is the one that
  declares the material block; a declaration the shader does not use is
  not in the reflection, and its annotations describe nothing.
- **`ShaderData`** (resource kind `shader`, `shader/ShaderData`) is data
  only: the device description (SPIR-V and reflection), the material
  layout (fields with type, offset, size, attributes, default), the
  textures (name, binding, default). `buildShaderData`
  (`shader/ShaderDataBuild`) makes it: compile, profile, engine blocks, the
  material set rule (the `Material` block at binding 0, the rest textures)
  and the annotations. The errors of the last compile are in the resource
  status (`ResourceStatus::message`), not in the data.
- **`ShaderLoader`** for `.yshader` (`kShaderFileExtension`): compiler
  paths are relative to the project folder, so errors name the project
  file and an include cannot leave the project; `<yqlora/...>` comes from
  the engine. A failed compile is the resource error with all messages.
- **Dependencies of a resource**: `LoadResult::dependencies` lists the
  files read besides the resource file, also on error; `reloadChanged`
  looks at them too, so a changed include recompiles the shaders using it.
- **Tangents in the profile**: location 3, `vec4`; `Vertex` gets `tangent`
  (exact for the built-in meshes), `meshVertexLayout` passes it, glTF
  `TANGENT` is read when present (computing them is phase 5.7);
  `standard.glsl` passes `vTangent`.
- **`.meta` of a shader** records the profile version (`profileVersion`)
  through `LoadResult::importUpdate` on import (`MetaPolicy::Import`); a shader of another version does not
  load until the project is upgraded.
- **Built-in resources** `Standard` and `Error` (`builtin::kStandardShader`,
  `builtin::kErrorShader`, `shader/BuiltinShaders`) from `engine:`. All
  built-in resources (meshes and shaders) are one table
  (`resource/BuiltinResourceTable`) and are made on first use: only those
  who need the shaders compile them. Until phase 5.3 `MeshPass` compiles
  `standard.yshader` itself; with materials it takes the shader from the
  cache.
- The C++ engine blocks (`FrameUniforms` and the others) moved to
  `render/EngineUniforms`: the name `ShaderData` now belongs to the
  resource.
- **Errors in the log** as one block per compile: "Resource '...' is not
  loaded: the shader does not compile:" and `file:line: text` lines.
- **"Create resource > Shader"** from `assets/templates/shader/Shader.yshader`
  through the window with a name and "Create"; a shader icon in Files.
- **Tests**: annotations on their own lines and trailing; an annotation
  with no declaration and an unfinished one are errors with the line;
  unknown ones, wrong arguments, ones not fitting the type and repeated
  ones are rejected with the line; an extra block in the material set is
  rejected; the template compiles; the built-in shaders are in the cache;
  the loader gives both stages of one file and writes the profile version
  into the `.meta`, and does not load another version; an error inside an
  include names that file and line; a changed include reloads the shader;
  a new shader is a copy of the template.

### 5.3 Materials [done]

- **`MaterialData`** (`engine/internal/yqlora/material`, resource kind
  `material`, format `Material` version 1): shader, `blend`, `cull`, values
  and textures by name (`materialValue`, `materialTexture`); reading and
  atomic writing of `.ymat` (`MaterialFile`), `MaterialLoader`; values the
  shader lacks are kept, a value of another size is not used.
- **Built-in materials** `Default` (`Standard`, white) and `Error` (the
  `Error` shader) in `builtinResourceTable()`; Extract draws an empty slot
  with `Default` and a missing material, or one whose shader is broken or
  missing, with `Error`.
- **Frame materials**: Extract gives `FrameData::materials` (each material
  once, with its shader and its textures already found) and one `DrawItem`
  per submesh with its index range and material index; the renderer does
  not read the resource cache.
- **GPU**: `GpuTextureCache` (a texture imported as sRGB becomes
  `Rgba8Srgb`, so sampling decodes it; default textures `white`, `black`,
  `normal`; samplers by import settings), `GpuMaterialCache` (the
  `Material` block from `materialBlock`: the offsets of the shader
  reflection, `color` values from sRGB to linear, ints as ints; keyed by
  material ID and the revisions of the material and its shader),
  `PipelineCache` (one pipeline per shader revision, blend, cull and target
  formats; a new shader revision drops the pipelines of the old one; a
  shader the backend does not build draws with `Error`). They are in
  `RenderContext`, shared by every view.
- **Maps of `Standard`** (see "Decisions"): `baseColorMap`, `ormMap`,
  `normalMap`, `heightMap`, `emissionMap`, the factors `emission`,
  `occlusionStrength`, `normalScale`, `heightScale`; `normalFromMap` and
  parallax occlusion mapping `parallaxUv` in `standard.glsl`.
- **Slots**: `MeshRenderer::materials`, a list of material references (the
  `resource(material)` attribute); the colour is gone.
- **Overrides for scripts**: `MeshRenderer::overrides(slot)` with
  `MaterialOverrides`; Extract copies them into `FrameData::overrides`
  with the textures they replace, `MeshPass` writes the material data of
  such an item into the frame uniforms; an override that does not fit the
  shader is skipped with one message per shader and field.
- **Prepare**: opaque items grouped by pipeline and material, front to back
  inside a group; transparent ones after them, back to front.
- **Tests**: the `.ymat` format; values by name, defaults and a value of
  another type; built-in and project materials as resources; the
  `Material` block, overrides and `clear`; one pipeline per state and new
  pipelines for a new shader revision; the order of Prepare; Extract: a
  slot, `Error` for a broken shader and back after a fix, the default
  material, copied overrides; an old scene with `color` opens; on OpenGL:
  an sRGB texture is decoded, an override changes one of two cubes with
  one material.

### 5.4 Materials in the editor [done]

- **"Create resource > Material"** (`ProjectFiles::createMaterial`, a row
  of `newResourceTypes()`): `Standard`, white, the window with a name.
- **Material inspector** (`MaterialInspector`): the preview, the shader
  field (shaders only), blend and cull, the fields of the `Material` block
  with the widgets of their annotations (colour, range, integer, vector),
  texture fields (textures only, with a preview). An edit goes through
  `MaterialEditing`: the material in the cache is replaced at once
  (`ResourceCache::replace`, a new revision), so every object with it and
  the preview change while a slider moves; the file is written atomically
  when the edit ends and at most `kWritesPerSecond` (2) times a second
  during a long drag, and when the project closes.
- **Preview** (`MaterialPreview`): a sphere with the material lit by one
  fixed directional light and the ambient light, in its own render target
  at the top of the inspector; its scene (camera, sphere, light) is its
  own, not the document.
- **Shader inspector** (`ShaderInspector`): compiled or the errors as
  lines, the declared fields and textures, "Open". A click on an error
  opens its file with the system program; the system cannot open a file at
  a line.
- **`MeshRenderer` inspector**: a row per submesh named after the material
  of the source file (`SubMesh::sourceMaterialName`) or "Element N", with a
  material field. Rows come from `Property::ElementLabels`, a general means:
  a component gives the labels of the elements of its list property.
- **Dropping a material** from Files onto a slot, or onto an object in the
  viewport: `raycastSurfaces` returns the submesh under the cursor and
  `SceneEditing::dropMaterial` sets the slot in one undo step (a change of
  the scene, unlike edits of the material file).
- **A material from a script**: `SharedMaterial` changes the material in
  the cache for all objects at once, through `ResourceCache::replace`, as
  the inspector does, without writing the file.
- **Resource kinds for scripts**: `kinds::kMaterial`, `kinds::kShader`,
  `kinds::kTexture` and the others are public
  (`yqlora/resource/ResourceKinds.hpp`), the data types take their kind
  from there, so a script field can hold a material, a shader or a texture:
  `field("look", &Player::look).resource(kinds::kMaterial)`.
- **Tests**: slot names from glTF and the labels of the `materials`
  property; dropping onto an object sets the right slot and is undone; an
  edit reaches the cache at once and writes the file when the edit ends or
  twice a second, not every frame; the preview scene; a shared material
  changes the cache and not the file; resource fields of a script know
  their kind.

### 5.5 Sky and environment

- **RHI for the sky**: a depth compare op in the pipeline (`<=`: the sky
  lies at depth 1, and the comparison is always `<` now), a cube texture
  and its binding in OpenGL (any texture is bound as 2D now).
- **`<yqlora/sky.glsl>`**: the stages of a sky shader around
  `sky(direction)` (a full screen triangle at depth 1, the direction from
  the inverse view projection) and the `YQLORA_SKY_AMBIENT` variant for the
  ambient pass. A sky shader is told by its reflection (no vertex inputs,
  has `sky`), and a sky material takes only such a shader.
- **Built-in `Procedural` and `Panoramic`** (`builtin::kProceduralSkyShader`,
  `builtin::kPanoramicSkyShader`) and the `Default Sky` material.
- **The sun**: the `sun` flag on `DirectionalLight3D`, the sun chosen in
  Extract (flagged, otherwise the brightest), `sunDirection`, `sunColor`,
  `hasSun` in the frame block.
- **`CameraClear::Sky`**, the default of a new camera; `SkyPass` after the
  opaque pass in the same view.
- **`LightingSettings`**: `skybox`, `ambientSource`, `ambientIntensity`,
  the gradient colours and the colour; `ambientLight(normal)` for shaders.
- **`AmbientPass`**: 9 coefficients into a 9 x 1 `Rgba16Float` texture,
  keyed by the revisions of the sky material and its shader and by the sun;
  the coefficients are read in `lighting.glsl`.
- **Editor**: the sky toggle of the viewport, the sky in the material
  preview, "Create resource > Sky shader" from `assets/templates/sky`.
- **Tests**: Extract picks the flagged sun, without a flag the brightest;
  the sky template compiles, a shader without `sky` is refused as a sky
  material; on OpenGL: a `Sky` camera draws the sky only where there are no
  objects, the harmonics of a constant sky give its colour along any
  normal, `Procedural` near the horizon is redder at sunset than at noon;
  `Gradient` lights the top of a cube with the sky colour and the bottom
  with the ground colour.

### 5.6 Shadows

- **RHI**: `rhi::Format::Depth32Float` readable by shaders, a comparison
  sampler (`SamplerDesc::compare`), `sampler2DShadow` in the profile and
  the reflection; depth-only passes (the colour target becomes optional in
  `RenderPassDesc`, `RenderGraph::Pass` and the test device); depth bias in
  the pipeline; a viewport over part of the target (2 x 2 cascades, atlas
  tiles); the OpenGL backend.
- **Properties**: `shadows`, `shadowStrength`, `shadowBias`,
  `shadowNormalBias` on `Light3D`; `castShadows`, `receiveShadows` on
  `MeshRenderer`; distance, cascades and map sizes in `LightingSettings`.
- **Extract**: which lights get shadows, the cascades of the directional
  light (splits, texel-snapped matrices), atlas tiles of spot lights and
  point light faces; per map lists of shadow casters culled by its
  frustum.
- **`ShadowPass`**: the cascade map and the atlas, a depth pass per
  cascade and tile, before the opaque pass in the graph; the frame block
  gets the matrices and the splits.
- **`lighting.glsl`**: cascade selection with blending, `Hard` and `Soft`
  (PCF over a rotated Poisson disc), biases; `receiveShadows` through the
  object block.
- **A new scene**: the sun with `Soft`; the editor shows the cascades in a
  debug view of the viewport.
- **Tests**: cascade splits grow and cover the distance; texel snapping
  keeps the shadow in place when the camera moves less than a texel; tile
  limits and the nearest kept; on OpenGL: a cube above a plane darkens the
  plane below it, without `castShadows` it does not, `receiveShadows` off
  on the plane removes the shadow from it; a point light casts shadows in
  every direction; the ambient light stays in the shadow.

### 5.7 glTF materials

- **Images of a model** become nested textures (`texture:Name`), from a
  file next to the model or from a buffer of the GLB, with the colour space
  taken from their use (base colour and emission sRGB, the rest linear).
- **Materials of a model** become nested materials (`material:Name`) of the
  `Standard` shader: base colour factor and texture, metallic and roughness
  factors and texture (into `ormMap`), occlusion texture and strength (the
  R of the same `ormMap` when it is one image, as is usual in glTF;
  otherwise the images are packed into one on import), normal texture and
  scale, emission with `KHR_materials_emissive_strength`, `alphaMode`
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

### 5.8 Documents

This file, rendering (lighting, sRGB, Prepare with materials), resources
(shaders, materials, dependencies), scene (light nodes, `MeshRenderer`
slots and overrides), project (formats of `.yshader`, `.ymat`, scene 2,
project upgrade steps), project settings (Lighting), editor, the guide
"Your own shader, material and sky", README. After this phase step 5 is closed.
