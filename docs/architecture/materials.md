# Shaders, materials and lights

[Русская версия](materials.ru.md)

Step 5 of the MVP: shaders written by the project, materials that give a
shader its values and textures, and light sources in the scene. Built on the
[RHI stage](rendering.md#the-rhi-stage-done): shaders go through the same
compiler, profile and reflection as the built-in ones.

Status marks: see [README](README.md).

## Decisions [done]

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
  - `emissionMap` (sRGB) times the colour `emission` and
    `emissionIntensity` (see "Light units, exposure and tone mapping");
  - `alphaCutoff`: fragments whose alpha is below it are discarded, at 0
    nothing is.
  Occlusion darkens only the ambient light, not direct lights (as in glTF).
  Separate occlusion and metallic-roughness maps of glTF are packed into
  one on import (see "glTF materials"); detail maps, anisotropy, clear coat and subsurface scattering come
  after the MVP with photorealism.
- **Colour lives in the material.** `MeshRenderer` has no colour of its
  own. Until there are users, file formats stay at version 1 without
  upgrades (see [project](project.md#format-versions-done)), so a scene
  written before materials draws its objects with the default material,
  and their `color` stays an unknown property.
- **Material edits go straight into the file**, without
  pressing "Save"; the edit history of the scene does not hold them. Undo
  for materials can come later; assigning a material in the scene is a
  scene edit and is undone as one.
- **Forward rendering, every light per pixel**, with shadows from the
  sun, spot and point lights (see "Shadows"); post-processing comes after
  the MVP (its place in the graph is ready).
- **Physical light**: lights, emission and the sky in physical units,
  the exposure of the camera and tone mapping (see "Light units, exposure
  and tone mapping").
- **Sky and sun**: the sky is a material with a sky shader
  (the built-in `Physical`, `Panoramic`, `Gradient` or your own), the sun
  is a directional light; the sky, the ambient light and the atmosphere
  belong to the `Environment` node of the scene; the sky lights the scene
  as ambient light and reflections (see "Environment" and the sections
  after it).
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
     `kOutputColorFormat`), which ImGui shows unchanged. Tone mapping is in
     the same pass, before the encoding.

  Passes of a view's owner (the grid, the outline) draw over the final
  target after the output pass, with colours given in sRGB, and still test
  the depth of the scene. Writing into `Rgba8Srgb` targets
  (`GL_FRAMEBUFFER_SRGB`) is not used: ImGui would decode such a texture
  when showing it, and OpenGL 3.3 has no core way to skip that.

## Shader files [done]

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
  `Vertex` is position, normal, `uv0`, tangent. The profile is at
  version 1; a shader file records the profile version in its `.meta` on
  import in the editor.
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
renderer.overrides(0).clearAll();
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
- A material can also be changed from a script for all objects at once,
  through `SharedMaterial(services, material)`: `setFloat`, `setVector`,
  `setColor`, `setTexture`. Such a change is not written to disk; built-in
  materials do not change (the setters return false).

## Lights [done]

- **Nodes** (engine nodes, see [scene](scene.md)): `DirectionalLight3D`,
  `PointLight3D`, `SpotLight3D`, all `Node3D`. Common: colour (sRGB, shown
  as a colour), intensity in lux or candela (see "Light units, exposure and
  tone mapping"). Point and spot: range. Spot: inner and outer
  angle. The direction is the `-Z` of the node, like a camera.
- **Extract** puts the lights of the scene into `FrameData::lights` (world
  position and direction, linear colour times intensity); a disabled node
  gives no light.
- **The frame block** holds the sun, the ambient multiplier, up to 4 directional lights
  and up to 32 point and spot lights together, with their counts. More
  lights than that: the nearest to the camera of the first view are kept
  and the log says so once. A near light may still miss an object; per
  object light lists come later.
- **Falloff** of point and spot lights is inverse square with a smooth
  window to zero at the range, as in glTF `KHR_lights_punctual`:
  `intensity / max(d², ε) * saturate(1 - (d / range)⁴)²`; a spot also fades
  smoothly between the inner and outer angle.
- **Ambient light and reflections** come from the `Environment` of the
  scene (see "Ambient light and reflections"); shadows of every kind of
  light are in "Shadows".
- **A new scene** has a camera, the sun and an `Environment`, so a new object is lit at once.
- **The editor** draws light icons and their range or direction in the
  viewport, as it does for cameras.

## Light units, exposure and tone mapping [done]

Light is given in physical units from the start, so that scenes, imported
models and the sky agree with each other and nothing is converted later.

- **Units** (the units of glTF `KHR_lights_punctual`, so lights of a model
  come in as they are):
  - `DirectionalLight3D::intensity`: illuminance in lux. A new light has
    100000 lux (the sun above the atmosphere is about 128000; with the
    atmosphere on, the light of the sun is dimmed by it on the way, see
    "Atmosphere");
  - `PointLight3D` and `SpotLight3D`: luminous intensity in candela (a 100 W
    bulb is about 130 cd, the default); the spot has it along its axis;
  - emission of a material: luminance in nits (cd/m2), `emission` (a
    colour) times `emissionIntensity`;
  - the sky, the gradient and the colour of the environment: luminance in
    nits.
- **Lighting without fudge factors**: a white diffuse surface facing a light
  of E lux has the luminance E / pi; a point light of I candela gives
  I / d2 lux at the distance d.
- **Exposure of the camera**: `Camera3D::exposure` in EV100 (15 by default:
  a sunny day), the factor is `1 / (1.2 * 2^EV100)`. The exposure is
  applied in the shaders before writing the HDR target (pre-exposure), so
  the sun and its disc stay within `Rgba16Float`. The editor viewport uses
  the exposure of the main camera of the scene, so the editor shows what
  the game shows. Auto exposure comes after the MVP as one more mode of the
  same property.
- **Tone mapping** in the output pass: `Camera3D::toneMapping` (`AgX` by
  default, `Aces`, `Neutral` (Khronos PBR Neutral), `None`), taken from the
  main camera, since all views of a frame share one HDR target. After tone
  mapping comes the sRGB encoding, as now.

## Environment [done]

The sky, the ambient light and the atmosphere of a scene are a node of the
scene, so every scene (a day level, a night level) has its own and edits
are undone like any other.

- **`Environment` node** (an engine node, not spatial). The first active one
  in the scene is used (`RenderSystem` keeps the list, as for cameras); a
  scene without it has no sky, no ambient light and no atmosphere. A new
  scene has one, with the defaults below.
- **Properties**:
  - `sky`: a material with a sky shader (the built-in `Default Sky` by
    default); empty: no sky, a `Sky` camera clears to black;
  - `ambientSource`: `Sky`, `Gradient`, `Color`; `ambientIntensity`, a
    multiplier (1 is physical); they light the scene (see "Ambient light and reflections");
  - for `Gradient`: `skyColor`, `equatorColor`, `groundColor` (sRGB) and
    `gradientLuminance` in nits; for `Color`: `ambientColor` and the same
    luminance;
  - `atmosphere`, a record (`Atmosphere`, shown as a nested table):
    `enabled`, `groundAlbedo`, `planetRadius` and `atmosphereHeight` in km,
    `rayleighScattering` (per megametre, RGB) and `rayleighHeight` (km),
    `mieScattering`, `mieAbsorption` (per megametre), `mieHeight` (km),
    `mieAnisotropy`, `ozoneAbsorption` (per megametre, RGB) and
    `aerialPerspective` (0 no haze, 1 physical, more for a style). The
    defaults are the Earth; every value can be changed or the whole
    atmosphere turned off.
- **The sun** is the directional light with the `sun` flag; when no enabled
  light has the flag, the brightest directional light is the sun. Extract
  puts it first among directional lights (`LightData::sun`), the frame
  block gets `hasSun`, `sunDirection`, `sunColor`, so any shader knows
  where the sun is.
- **Reports once**: a sky material whose shader is not a sky shader gives
  no sky and one message in the log per scene (`RenderSystem::takeReport`,
  the same as for too many lights).
- **The `Sky` camera clear** (`CameraClear::Sky`, the default of a new
  camera): the sky pass draws the sky material after the opaque items and
  before the transparent ones, on the far plane (depth test `<=`, no
  write), so it fills only the pixels where nothing is.

## Sky shaders [done]

- **A sky shader** is an ordinary `.yshader` that includes
  `<yqlora/sky.glsl>` and defines one function `vec3 sky(vec3 direction)`:
  the luminance of the sky in nits in a direction (world axes, unit). The
  library gives `main` of both stages (a full screen triangle on the far
  plane, the direction of the pixel from the inverse view projection) and
  applies the exposure. Including `sky.glsl` is what makes a shader a sky
  shader (`ShaderData::kind`); a sky material takes only such a shader, and
  a mesh slot with a sky material draws with `Error` (`FrameMaterials`
  checks a material by where it stands). `sky()` is called only in the
  fragment stage; a sky that uses `dFdx` wraps its definition in
  `#ifdef YQLORA_FRAGMENT`.
- **The capture variant**: the engine builds every sky shader a second time
  with `YQLORA_SKY_CAPTURE` defined, for lighting (see "Ambient light and
  reflections"). In it a sky leaves out what lights already give, such as
  the disc of the sun.
- **Built-in sky shaders** with fixed IDs, like `Standard`:
  - `Physical` (`builtin::kPhysicalSkyShader`): the sky of the atmosphere
    of the environment, with the disc of the sun: `sunDisc` (on or off),
    `limbDarkening`, `sunDiscIntensity` (1 is physical), `sunDiscSize`
    (the angular size seen, in degrees, 3.5 by default; 0 takes the angular
    diameter of the light, `DirectionalLight3D::angularDiameter`) and
    `sunDiscSoftness` (the outer part of the radius where the edge fades,
    0.3 by default; 0 is a hard edge). The luminance of the disc is the
    illuminance of the sun divided by the solid angle of the disc, dimmed by
    the atmosphere, so a larger disc gives the same light and is less
    blinding. The size of the disc is only how it looks: shadows and the
    light of the sun use the size of the light. `Default Sky` (`builtin::kDefaultSkyMaterial`) is this
    shader with its defaults;
  - `Panoramic` (`builtin::kPanoramicSkyShader`): an equirectangular image
    `panorama` (8 bit sRGB or HDR `.hdr`), `rotation` around the vertical,
    `tint` and `luminance` in nits for the value 1; mips are picked
    without a seam where the image wraps;
  - `Gradient` (`builtin::kGradientSkyShader`): `skyColor`,
    `equatorColor`, `groundColor`, `luminance` and `falloff` (how wide the
    horizon band is); the `Gradient` and `Color` ambient sources light the
    scene through it.
- **Exposure**: `<yqlora/exposure.glsl>` gives `exposed(luminance)`, used by
  `shade()` and the sky: the exposure of the view, clamped below the
  largest half float so a bright sun never becomes infinity.
- **Clouds** [planned, after MVP] are part of the built-in `Physical`,
  with their own switch and parameters; your own clouds are still written
  in your own sky shader. First a layer at a height: coverage, density,
  height, wind (an offset over time), shape from noises
  ([noise](noise.md)), lighting by the sun and the sky, forward scattering
  near the sun. Then volumetric clouds: a ray march through the layer with
  the same light, cloud shadows on the ground (through the sun shadows) and
  their part in the sky light. Clouds change the sky capture, so with wind
  it repeats at a limited rate rather than every frame.

## Night sky: stars and the moon [planned, after MVP]

Together with clouds and auto exposure: a night scene needs all three.

- **Stars** are part of the built-in `Physical`, with their own switch:
  procedural by default (no files), with intensity and twinkle; an own
  panorama (`.hdr` or an image) may replace them. The star sphere turns
  around a celestial pole (axis and speed are parameters). Their luminance
  is physical, so they vanish in a bright day sky by themselves and show
  at a low exposure.
- **The moon** is a second directional light: `DirectionalLight3D` gets a
  role (sun, moon, plain light). The sky draws its disc with the phase
  from the direction of the sun, optionally with a texture; moonlight is
  physical (about 0.1 to 0.3 lux) and casts ordinary shadows.
- **The atmosphere scatters the light of both bodies**, so a moonlit night
  sky is dark blue, not black; the tables are computed for two bodies.
- **Where the engine ends**: the engine draws stars and the moon, its
  phases and scattering; the game moves them in time (the day cycle, a
  calendar) through the API of lights and the sky.

## Atmosphere [done]

The physical sky by the method of Hillaire ("A Scalable and Production
Ready Sky and Atmosphere Rendering Technique", 2020): single scattering
computed by ray marching, multiple scattering from a small table,
Rayleigh, Mie and ozone. All tables are 2D and are filled by fragment
shaders, so every backend can do it.

- **Tables** (`Rgba16Float`):
  - transmittance, 256 x 64: by height and the angle to the zenith; filled
    when the atmosphere of the environment changes;
  - multiple scattering, 32 x 32: by height and the angle to the sun;
    filled after the transmittance;
  - the sky view, 192 x 108: the sky around the camera for its height and
    the sun, every frame per view; the horizon gets more rows;
  - aerial perspective, 32 slices of 32 x 32 side by side (1024 x 32):
    in-scattering and transmittance along the view of the camera up to
    32 km, slices denser near the camera (the distance grows as the square
    of the slice), every frame per view.

  `AtmospherePass` owns the first two between frames and fills them again
  only when the `Atmosphere` block changes; the other two are transient
  textures of the graph. Without an atmosphere there are no passes, and
  shaders get a 1 x 1 table that changes nothing. The mappings of the
  tables live in `atmosphere.glsl`, the ray marching shared by the passes
  in `engine/shaders/atmosphere/march.glsl`.
- **`<yqlora/atmosphere.glsl>`** gives every shader the functions of the
  atmosphere: `atmosphereSky(direction)`, `atmosphereTransmittance(position,
  direction)`, `applyAerialPerspective(color, position)`. The `Atmosphere`
  block (set 0, binding 1) holds its parameters in shader units (km, per
  km); the transmittance table is set 0, binding 2, the sky view and aerial
  perspective tables set 1, bindings 1 and 2. `bindEngineResources` gives a
  shader only the blocks and tables it declares.
- **The sun through the atmosphere**: `lighting.glsl` dims the light of the
  sun at a point by the transmittance from that point towards the sun, so
  the sun turns yellow and red near the horizon by itself.
- **Aerial perspective**: `shade()` applies it to every lit surface, opaque
  and transparent, so far objects fade into the haze of the sky colour.
- **Heights**: the scene origin lies on the ground of the planet, the Y of
  the camera is its height (at least 1 m above the ground).
- Clouds and volumetric fog come after the MVP, on top of the same
  tables.

## Ambient light and reflections [done]

The sky lights the scene twice: as diffuse ambient light and as
reflections on metals and glossy surfaces (image-based lighting).

- **Capture**: when the sky changes (its material, shader, textures, the
  sun, the atmosphere, the height of the camera by steps of 100 m, the
  ambient source and colours), the sky is drawn into a cube texture, 128 x
  128 per face, with the capture variant of its shader. `Gradient` and
  `Color` draw the built-in `Gradient Sky` material
  (`builtin::kGradientSkyMaterial`) with the colours of the environment as
  overrides into the same cube, so all three sources take one path. Each
  pixel of a face takes its direction from the cube face table shared by
  every API (`<yqlora/cube.glsl>`, the `Capture` block at set 3, binding
  1), not from a camera, so faces never come out flipped on another
  backend. `EnvironmentLighting` keeps the tables between frames and
  compares a key of all those inputs field by field. There are four sets of
  tables (`EnvironmentLighting::kSets`), each with its own key: frames with
  different skies (the scene, the material preview, the game view from
  another camera height) come in turn without new captures; a new sky gets
  the set unused for the longest time.
- **Diffuse**: 9 coefficients of second order spherical harmonics are
  projected from a small mip of the cube into a 9 x 1 texture (set 0,
  binding 3); `ambientLight(normal)` in `lighting.glsl` reads them. The
  basis is one file, `<yqlora/harmonics.glsl>`, for the projection and the
  lookup.
- **Specular**: mips 0..5 of a second cube (set 0, binding 4) are
  filtered by GGX for roughness 0..1 (filtered importance sampling); a 2D
  table of the split-sum BRDF (128 x 128, set 0, binding 5) is made once.
  `lighting.glsl` adds the reflection with the energy compensation for
  multiple scattering in rough metals; the occlusion of the material dims
  it too. The GGX functions are one file, `<yqlora/brdf.glsl>`, for the
  lighting and the tables.
- **Without sky light** (no environment, no sky for the `Sky` source) the
  tables are cleared to black once.
- **Multiplier**: `ambientIntensity` of the environment scales both, in the
  frame block (`ambientIntensity`, with `environmentMaxMip`).
- **Emission and exposure**: `Surface::emissionExposureWeight` (`Standard`:
  `emissionExposureWeight`, 0..1). 1 is physical, nits under the exposure of
  the camera; 0 keeps the emission as bright on screen at any exposure
  (signs, interface in the world, a cartoon style); values between blend.
- Everything stays on the GPU, with no read back, so a day and night cycle
  does not stall the frame.
- Reflection probes (local cubes placed in the scene) come after the MVP
  and use the same filtering.

## Shadows [done]

Lights give shadows, their meshes cast and receive them.

- **Lights**: every light has `shadows` (`None`, `Hard`, `Soft`),
  `shadowStrength` (0..1), `shadowSoftness`, `shadowBias` and
  `shadowNormalBias` (in map texels, so they do not depend on its size or
  cascade: against shadow acne and peter panning). A new
  `DirectionalLight3D` and the sun of a new scene use `Soft`, point and
  spot lights use `None`.
- **Meshes**: `MeshRenderer::castShadows` and `receiveShadows`, both on by
  default.
- **The sun** uses cascaded shadow maps: up to 4 cascades in one depth
  texture (2 x 2, a single cascade takes the whole texture), splits by a
  mix of the logarithmic and uniform schemes (`cascadeSplits`). A cascade
  is a sphere around its slice of the camera frustum (its size does not
  change when the camera turns), its centre is snapped to map texels (the
  shadow does not shimmer when the camera moves), its box reaches towards
  the sun up to the shadow casters (`fitShadowCascades`). At the far edge
  of a cascade it blends into the next one, at the shadow distance it
  fades out. One directional light has shadows: the sun (see
  "Environment"). Cascades are computed per view, for its camera: the
  editor viewport gets its own.
- **A spot light** uses one perspective map, **a point light** six cube
  faces; both are tiles of a shared 4 x 4 atlas (`kMaxShadowTiles`), one
  per frame. The lights nearest to the first camera get the tiles
  (`extractShadowTiles`), the rest stay without shadows and the log says
  so once.
- **Light size**: `DirectionalLight3D` has the angular diameter
  `angularDiameter` (0.53 degrees), point and spot lights the radius
  `sourceRadius` in metres (0.05). The sun disc in the sky (`Physical`)
  takes the same angle from the frame block (`sunAngularRadius`).
- **Softness**: `Hard` is one depth comparison with a linear filter (four
  texels). `Soft` is a penumbra by light size (PCSS): the average depth of
  the blockers around the point gives the penumbra width (for the
  orthographic sun map 2 tan(radius) per metre from the blocker, for a
  perspective one the light radius times the ratio of distances), then a
  filter of comparisons of that width. Samples lie on a Vogel disc (golden
  angle) rotated per pixel: even for any count and with no banding.
  `shadowSoftness` scales the width (1 is physical, 0 is sharp, more for
  style).
- **Passes** (`ShadowPass`): the atlas once per frame, the cascade map per
  view, both before the view in the graph; a depth-only pass, a viewport
  per cascade and tile, one depth shader (`shadow/depth.vert`) with a slope
  bias in the pipeline. Opaque meshes with `castShadows` are drawn, each
  only into the maps whose frustum it touches; view layers do not apply to
  shadows. Without shadows a map is 1 x 1 and cleared: lighting shaders
  always get both.
- **Shaders**: `<yqlora/shadows.glsl>` (included by `lighting.glsl`) holds
  the `Shadows` block in the view set (binding 3) and the maps (4-7:
  cascades and atlas, each with comparison and raw). `shade()` dims the
  direct light of a light by its shadow, shadows do not dim the ambient
  light; `receiveShadows` comes through the `Object` block.
- **RHI**: `Format::Depth32Float`, `SamplerDesc::compare` and
  `sampler2DShadow` in the profile, a pass without colour, `setViewport`
  inside a pass, `PipelineDesc::depthBias`.
- **Settings** (Graphics section): `shadows` turns shadows on at all (off
  means no shadows and nothing drawn into maps); `shadowDistance` (100 m),
  `shadowCascades` (4), `shadowCascadeSize` (2048) and `shadowAtlasSize`
  (4096), `shadowBlockerSamples` (16) and `shadowFilterSamples` (32),
  `shadowMaxPenumbra` (the limit of the penumbra width and of the blocker
  search, 3 m). The limit is in metres, not texels: the penumbra width is
  the same in every cascade and with any map size, quality changes only
  how clean it is, and the sample counts set the cost. Quality
  levels for the player settings menu come with the player interface and
  the standalone runtime (steps 8 and 9) and change these same values.
- **Editor**: a viewport button tints lit surfaces by the colour of their
  sun cascade (`ViewData::showShadowCascades`): it shows where each
  cascade ends and where shadows stop.
- **After the MVP**: contact shadows, shadows from transparent surfaces, ray
  tracing. Shadows of alpha-tested materials come with
  [glTF materials](#gltf-materials-done).

## Baked lighting [planned, after MVP]

Light and shadows of static objects are computed ahead, in the editor, and
stored in textures: cheap in the game, with soft shadows and bounced light
that shadow maps do not give.

- **Static**: a `static` flag on `MeshRenderer` (the object does not move
  in the game); only such objects get lightmaps and cast baked shadows.
- **A second UV set** (`uv1`): from the model file or built on import by a
  ready library (xatlas, MIT), with padding between charts.
- **Light mode** `mode` on `Light3D`: `Realtime` (as now), `Baked` (only
  in lightmaps, costs nothing in the game), `Mixed` (direct light in the
  game, shadows of static objects and bounced light baked; close by,
  dynamic shadows from the shadow maps, further away, the baked ones).
- **Baking** in the background with a button in the "Lighting" window: ray
  tracing (direct light, shadows of lights with a size, bounced light over
  several bounces, sky light), denoising with a ready library (Open Image
  Denoise, Apache 2.0). The result is HDR lightmap textures and the
  `Mixed` shadow mask next to the scene, referenced by the scene resource.
- **Light probes**: a probe group node in space; moving objects take
  diffuse light from the nearest probes (harmonics, as the sky light), so
  they fit into the baked scene.
- **Shaders**: `lighting.glsl` reads the lightmap by `uv1` when the object
  has one; a custom shader has nothing to do.

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
- "Create resource" also gets **Sky shader** (from the template
  `assets/templates/sky`: a colour from the horizon to the zenith, the sun
  disc with a soft edge, `sunDiscSize` and `sunDiscSoftness` as in
  `Physical`, and a glow around it).
- **The preview** has its own render target, camera, sphere, sun and a
  default environment (the sky, its light and reflections); nothing of it
  enters the scene, and its light does not light the scene. A sky material
  becomes the sky of the preview and the sphere turns white: both the sky
  and how it lights are visible.
- **Resource use**: a field may take not every resource of its kind
  (`.resource(kinds::kMaterial, uses::kSkyMaterial)`,
  `PropertyAttributes::resourceUse`). The `sky` field of an environment
  offers and takes only sky materials, `MeshRenderer` slots only surface
  materials (a sky material is not dropped onto an object in the
  viewport). There is one check, `fitsResourceUse`: the use of a material
  is the kind of its shader; a material with a broken shader fits both,
  otherwise it could not be picked until the shader is fixed.
- **The viewport** shows the scene sky behind objects; a button at the top
  right (`EditorSettings::viewportSky`, saved) turns it into the theme
  colour. Without a sky in the scene it is the theme colour. The sky lights
  the scene either way.
- **`Environment`** is in the "Add node" menu, with its own icon.
- **The shader inspector**: the result of the last compile (fine, or the
  errors), the material block and the textures it declares; "Open" opens the
  file in the system editor. Editing shaders inside the editor comes with
  the Scripts tab (see [code editor](code-editor.md)).
- **The `MeshRenderer` inspector** shows the slots by name; a material is
  dropped from Files onto a slot, or onto an object in the viewport (the
  slot of the submesh under the cursor).

## glTF materials [done]

- Materials and images of a model become nested resources of the file
  (`material:Name`, `texture:Name`), with IDs in `.meta` as meshes have.
  Images are read from a file next to the model (a dependency of the
  model: it changes, the model is read again), from a buffer of the GLB or
  from a data URI.
- **References inside a file**: a nested material refers to nested
  textures of the same file. The loader does not know their IDs (`.meta`
  gives them after the load), so a loaded resource lists its references
  by key, and the cache writes the IDs into the data before it is
  published. Without an ID in `.meta` (a read-only project) the reference
  stays empty and the texture by default is used.
- **Colour space by use**: base colour and emission sRGB, the rest
  linear. An image used both ways gives two nested textures. Filter and
  wrap come from the glTF sampler.
- **Values**: `baseColorFactor` is linear in glTF and is written as sRGB
  (colour fields of materials are sRGB). glTF emission has no units: the
  colour is the factor divided by its largest component (in sRGB),
  `emissionIntensity` is that component times
  `KHR_materials_emissive_strength`, and `emissionExposureWeight` is 0, so
  1 is white on screen at any exposure, as the file means it.
- **`ormMap`**: when occlusion and metallic-roughness are one image, it is
  used as is; otherwise they are packed into a new
  `texture:Material ORM` (R occlusion, G roughness, B metallic; the
  smaller image is resampled bilinearly to the larger size). Without an occlusion texture
  `occlusionStrength` is 0: the R of a metallic-roughness image may hold
  anything.
- **`alphaMode`**: `BLEND` is transparent; `MASK` is opaque with
  `alphaCutoff` of `Standard`, which discards fragments below it (a real
  alpha test, not a silent substitution). **`doubleSided`**: cull none,
  back faces are lit with the flipped normal and tangent frame
  (`gl_FrontFacing` in `standard.glsl`, for every shader).
- **Alpha-tested shadows**: a surface shader is also built with
  `YQLORA_DEPTH`, a depth-only variant (`ShaderData::depthDesc`): in it
  `shade()` of `lighting.glsl` computes nothing, so the compiler drops the
  colour work and only what the fragment discards before `shade()` is left,
  the alpha test. `ShadowPass` draws a caster whose material has an
  `alphaCutoff` field (any shader, by name) above 0, also from overrides,
  with the depth variant of its material and the material bound; the rest
  with the shared depth shader, solid. A leaf with `MASK` casts the shadow
  of the leaf, not of its quad.
- **Tangents** come from `TANGENT` of the file with their `w`; when a
  material of a submesh has a normal map and the file has none, they are
  computed with MikkTSpace (the library, as glTF requires): per triangle
  corner, and a vertex whose corners disagree (a UV seam, a mirrored half)
  is split.
- `ModelData::materials` holds the material keys by the material index of
  the file; a submesh finds its material through `SubMesh::sourceMaterial`.
- **Not supported, said in the log, nothing substituted silently**:
  `texCoord` 1 (`uv0` is used, with a warning); `KHR_texture_transform`,
  KTX2/basisu, WebP and other `KHR_materials_*` (one line per file with the
  list); `extensionsRequired` that are not supported (Draco) fail the load.
- A material of a model is read-only: its values come from the file, and
  `MaterialEditing` refuses nested resources (their path is the model
  file). To change it, the inspector offers "Make a copy", which writes a
  `.ymat` next to the model through the create window; the copy refers to
  the nested textures of the model and is selected. A texture of a model
  shows the sampling of the file, read-only.
- Dropping a model into the scene fills the slots of every `MeshRenderer`
  with the materials of the file (a submesh without a material leaves its
  slot empty); so does dropping one nested mesh. Files shows the materials
  and textures of a model under it, like its meshes; a click shows one in
  the inspector.
- Nested materials and textures keep their IDs in `.meta` like meshes: a
  re-export keeps the references of the scenes.

## Phases of step 5 (a commit per phase) [done]

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

### 5.5a Light units, exposure, tone mapping [done]

- **Lights**: intensity in lux and candela, the defaults above;
  `lighting.glsl` without the factor pi; `Standard` gets
  `emissionIntensity` in nits.
- **Camera**: `exposure` (EV100) and `toneMapping`; the View block gets
  the exposure and the inverse view projection, shaders apply the
  exposure (`shade`, the sky).
- **Output pass**: `AgX`, `Aces`, `Neutral`, `None` in
  `<yqlora/output.glsl>`, chosen by the main camera.
- **RHI**: the depth compare op of the pipeline (`Less`, `LessEqual`).
- **Tests**: a white surface facing 1000 lux at exposure factor 1 has the
  luminance 1000 / pi; a point light of 100 cd lights 1 m away with 100
  lux and 2 m away with 25; EV100 15 gives `1 / (1.2 * 32768)`; tone
  mapping keeps 0 at 0, is monotonic and stays below 1; `None` with factor
  1 keeps the old output.

### 5.5b Environment, sky shaders, atmosphere [done]

- **`Environment`** node with its properties, kept by `RenderSystem`; the
  `sun` flag of `DirectionalLight3D` and the choice of the sun in Extract;
  the `Lighting` section of the project settings is removed.
- **Shaders**: `<yqlora/sky.glsl>`, `<yqlora/atmosphere.glsl>`,
  `ShaderData::kind` and the capture variant; built-in `Physical`,
  `Panoramic`, `Gradient` and the `Default Sky` material.
- **HDR textures**: `.hdr` through stb_image, `TextureData` in `Rgba8` or
  `Rgba16Float`.
- **Passes**: the atmosphere tables, `CameraClear::Sky` and the sky pass
  between opaque and transparent items (`prepareView` tells where the
  opaque ones end), the sun dimmed by the atmosphere, aerial perspective in
  `shade()`.
- **Tests**: Extract picks the flagged sun, without a flag the brightest;
  a shader with `sky.glsl` is a sky shader and gets a capture variant; a
  sky material on a mesh draws with `Error`; on OpenGL: a `Sky` camera
  draws the sky only where there are no objects; the sky near the horizon
  is redder at sunset than at noon; the sun light on a white plane is
  dimmer and redder at sunset; a far object is closer to the sky colour
  than a near one.

### 5.5c Ambient light and reflections [done]

- **RHI**: cube textures (`TextureDesc::cube`), drawing into a face and a
  mip, `generateMipmaps`, `samplerCube` in the profile; the OpenGL backend
  and the test device.
- **Passes**: the capture into the cube, the projection into harmonics, the
  GGX filtering of the mips, the BRDF table; the frame set gets their
  textures; `ambientLight` and the specular term in `lighting.glsl`.
- **Emission and exposure**: `Standard` gets `emissionExposureWeight`
  (0..1): 1 is physical (nits under the exposure of the camera), 0 keeps
  the emission as bright on screen at any exposure (signs, interface in
  the world, a cartoon style), values between blend.
- **Tests**: the harmonics of a constant sky give its luminance along any
  normal; `Gradient` lights the top of a cube with the sky colour and the
  bottom with the ground colour; a smooth metal sphere reflects the sky,
  a rough one its average; the capture is not repeated while nothing
  changes.

### 5.5d Editor [done]

- The sky toggle of the viewport; the sky and the ambient light of the
  default environment in the material preview; "Create resource > Sky
  shader" from `assets/templates/sky`; `Environment` in the "Add node"
  menu with its icon; the `sky` field offers only sky materials.
- **Tests**: a new scene has a camera with `Sky`, the sun and an
  `Environment`; the sky template compiles; the sky field offers only sky
  materials, a mesh slot only surface ones; the preview shows a sky
  material as the sky; the scene and the preview in turn do not capture
  the sky again; the viewport sky flag is saved.

### 5.6 Shadows [done]

- **RHI**: `rhi::Format::Depth32Float` readable by shaders, a comparison
  sampler (`SamplerDesc::compare`), `sampler2DShadow` in the profile and
  the reflection; depth-only passes (the colour target becomes optional in
  `RenderPassDesc`, `RenderGraph::Pass` and the test device); depth bias in
  the pipeline; a viewport over part of the target (2 x 2 cascades, atlas
  tiles); the OpenGL backend.
- **Properties**: `shadows`, `shadowStrength`, `shadowSoftness`,
  `shadowBias`, `shadowNormalBias` on `Light3D`; `angularDiameter` on
  `DirectionalLight3D` (the sun disc angle moves from `Physical` to the
  frame block), `sourceRadius` on point and spot lights; `castShadows`,
  `receiveShadows` on `MeshRenderer`; the switch, distance, cascades, map
  sizes, sample counts and `maxPenumbra` in `GraphicsSettings`.
- **Extract**: which lights get shadows, the cascades of the directional
  light (splits, texel-snapped matrices), atlas tiles of spot lights and
  point light faces; per map lists of shadow casters culled by its
  frustum.
- **`ShadowPass`**: the cascade map and the atlas, a depth pass per
  cascade and tile, before the opaque pass in the graph; the frame block
  gets the matrices and the splits.
- **`lighting.glsl`**: cascade selection with blending, `Hard` and `Soft`
  (PCSS: blocker search, penumbra width, PCF over a rotated Vogel disc),
  biases; `receiveShadows` through the object block.
- **A new scene**: the sun with `Soft`; the editor shows the cascades in a
  debug view of the viewport.
- **Tests**: cascade splits grow and cover the distance; texel snapping
  keeps the shadow in place when the camera moves less than a texel; tile
  limits and the nearest kept; on OpenGL: a cube above a plane darkens the
  plane below it, without `castShadows` it does not, `receiveShadows` off
  on the plane removes the shadow from it; a point light casts shadows in
  every direction; the ambient light stays in the shadow; with `Soft` the
  penumbra is narrower where the object touches the plane than away from
  it, and wider for a larger light; the settings switch removes shadows
  and their passes.

### 5.7 glTF materials [done]

Four parts, a commit per part; the contract is in
[glTF materials](#gltf-materials-done).

- **5.7a Base**: `alphaCutoff` in `Standard` (0..1, default 0: fragments
  below it are discarded); back faces flip the
  normal and tangent frame in `standard.glsl`; `decodeImage(bytes,
  sampling)` taken out of `ImageLoader` for buffers of a model; MikkTSpace
  as a dependency and `computeTangents` with the split of vertices.
  Tests: a quad with a mirrored UV half gets opposite `w`; `alphaCutoff`
  discards on OpenGL; a back face of a double-sided material is lit with
  the flipped normal.
- **5.7b Loader**: references by key in `LoadedResource` resolved by the
  cache; nested textures (file next to the model, GLB buffer, data URI,
  sampler, colour space by use); nested materials (values, `ormMap`
  packing, emission, `alphaMode`, `doubleSided`); `ModelData::materials`;
  computed tangents; the log of what is not supported. Tests: a GLB with
  an embedded image gives the nested texture and material; `TANGENT` keeps
  its `w`; `MASK` gives `alphaCutoff`, `doubleSided` cull none; packed
  `ormMap` and `occlusionStrength` 0 without an occlusion texture;
  a re-export keeps the IDs and the material to texture references; read
  only without IDs leaves the reference empty.
- **5.7c Alpha-tested shadows**: the `YQLORA_DEPTH` variant of material
  shaders, `ShadowPass` choosing it by `alphaCutoff`. Tests on OpenGL: a
  quad with a cut-out texture casts the cut-out shadow; with `alphaCutoff`
  0 it casts the whole quad.
- **5.7d Editor**: dropping fills the slots (a model and one nested mesh);
  material and texture of a model read-only in the inspector and in
  `MaterialEditing`; "Make a copy"; Files shows nested materials and
  textures. Tests: dropping fills the slots; the copy holds the same
  values; editing a model material does not write the model file.

### 5.8 Documents [done]

This file, rendering (lighting, sRGB, Prepare with materials), resources
(shaders, materials, dependencies), scene (light nodes, `MeshRenderer`
slots and overrides), project (formats of `.yshader`, `.ymat`, scene 2,
project upgrade steps), project settings (Graphics), editor, the guide
"Your own shader, material and sky", README. After this phase step 5 is closed.
