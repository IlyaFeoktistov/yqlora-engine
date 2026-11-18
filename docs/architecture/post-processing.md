# Post-processing

[Русская версия](post-processing.ru.md)

Effects over the rendered picture of a camera: auto exposure, bloom,
anti-aliasing, depth of field, motion blur, colour grading, vignette and
grain, and a game's own effects; changed smoothly by zones in the scene.
This is the post-processing part of
[photorealism by default](rendering.md#photorealism-by-default-planned-after-the-mvp).

## Decisions [planned, after the MVP]

- **Each effect is its own record and its own pass.** An effect is a
  record of settings (`Bloom`, `AutoExposure`, `DepthOfField`, ...) with
  an `enabled` switch, and a render graph pass that reads them. A new
  effect is a new record and a new pass, and one line in the list of
  effects; nothing else changes. Every effect can be turned off and has
  all its parameters exposed; realistic values are only the defaults.
- **Settings come from a stack.** The settings of a camera's frame are
  computed each frame: the project defaults (Graphics), then the camera's
  own `PostProcessSettings`, then the volumes that cover the camera by
  priority, each blended by its weight. A field of a record is overridden
  only when its override flag is set, so a volume that changes only the
  bloom intensity touches nothing else. Blending is done on records by the
  registry (numbers and colours linearly, switches and enums at half of the
  weight): the same code for every effect, no per-effect blending.
- **Volumes are components.** `PostProcessVolume` on a `Node3D`: a shape
  (`Global`, `Box`, `Sphere`), `blendDistance` (the camera fades the volume
  in over that distance outside the shape), `weight`, `priority`, and the
  overridden records. A cave darker and bluer, underwater blur and colour,
  a drug effect by `weight` from code.
- **Where they run.** In the render graph of a view:
  ```
  opaque (+ motion vectors) -> sky -> transparent
    -> [HDR, linear] the luminance measure of auto exposure, TAA,
       depth of field, motion blur, bloom, custom "BeforeToneMap"
    -> output: tone mapping, colour grading LUT, sRGB
    -> [LDR] FXAA, vignette, grain, chromatic aberration, lens
       distortion, custom "AfterToneMap"
    -> HUD, editor passes
  ```
  Effects in HDR work in linear light; the ones after are a look on the
  final picture.
- **What the frame needs**, added to the renderer once for all effects:
  sampling the depth target in a pass; motion vectors (an `Rg16Float`
  target written by the opaque pass from the current and the previous
  world matrix of each item and the previous view projection); a jitter
  of the projection for TAA; history textures that live between frames of
  the same view (a persistent texture imported into each frame's
  `RenderGraph`); a mip chain made by a pass (downsampling for bloom and
  exposure); 3D textures for the grading LUT. Extract stays without
  memory: the previous matrices are kept by the renderer, per view, by
  node id and submesh. `Transform3D::teleport` (a new call: set the pose
  and mark the jump; the teleports of physics and rigs use it) makes the
  node's items get no motion that frame, so a cut leaves no smear.
- **Auto exposure without compute shaders.** The log luminance of the HDR
  picture is reduced by a chain of downsampling passes to one texel; the
  adapted value moves toward it at `speedUp` or `speedDown` and is kept in
  a 1 x 1 history texture. Shaders take the exposure from that texture
  through `exposure()` of `uniforms.glsl`, for manual exposure too (its
  value is written there), so there is one path and no branch. The frame
  is drawn with the exposure of the previous frame: one frame late,
  invisible at adaptation speeds. It is one more mode of the camera's
  exposure (`Auto` beside the manual EV), with `minEV`, `maxEV`,
  `compensation` and a metering mask (centre weighted by default). A
  histogram with compute shaders comes later behind the same settings.
- **Colour grading is a LUT.** White balance, lift, gamma, gain,
  contrast, saturation, hue shift, curves and an optional `.cube` LUT file
  are baked into one 3D LUT (32 cells) when the settings change, applied in
  the output pass after tone mapping. Changing ten grading values costs
  the same as one.
- **A game's own effects are materials.** A post shader is a `.yshader`
  with `#include <yqlora/post.glsl>`: it gets the colour of the previous
  pass, the depth, the motion vectors, the UV and the screen size, and
  writes a colour. A `PostProcessEffect` in a stack holds a material of
  such a shader, its point (`BeforeToneMap`, `AfterToneMap`), its order and
  its `intensity`: the result is mixed with the picture before it by
  `intensity`, which blends in volumes like any number. The material's own
  parameters do not blend: they are changed from code. An outline, a
  pixelated look, a heat haze, a scanner pulse: no engine code.
- **Editor and game the same.** The scene view uses the post-processing
  of the main camera, with the volumes taken where the editor camera is
  (a toggle in the view's menu, on by default); the game view and a built
  game use their camera's.

## The effects

- **`AutoExposure`**: above.
- **`Bloom`**: threshold with a soft knee, intensity, scatter, tint, a
  dirt texture with its intensity; a progressive downsample and upsample
  with a 13-tap filter (no flicker on small bright points).
- **`Antialiasing`**: `None`, `FXAA` (cheap, after tone mapping), `TAA`
  (jitter, reprojection by motion vectors, neighbourhood clamping, a
  sharpening amount). TAA is the default; an MSAA target is not used with
  post-processing.
- **`DepthOfField`**: focus distance (or a focus node), aperture (f-stop)
  and focal length (from the camera's field of view and its `sensorSize`,
  36 mm by default), so the blur follows a real lens; bokeh by a gather of
  a disc with a blade count; near and far blur separately.
- **`MotionBlur`**: intensity as a shutter angle, sample count, the
  camera's share and the objects' share separately.
- **`ColorGrading`**: above.
- **`Vignette`**, **`FilmGrain`**, **`ChromaticAberration`**, **`LensDistortion`**:
  intensity and their few shape fields.
- Ambient occlusion (SSAO), screen-space reflections and fog belong to
  the lighting part of the photorealism stage, not here; they plug into
  the same stack of settings.

## From code

- `Camera3D::postProcess()` - the camera's settings, every record and
  field, read and set.
- `PostProcessVolume`: `weight`, `priority`, shape, records; a game fades
  an effect by animating `weight`.
- `scene.postProcess().settingsAt(camera)` - the blended settings this
  frame (for a game's own effect that reads them).
- A custom effect's material is changed like any material
  ([materials from scripts](materials.md#materials-from-scripts-done)).

## Editor

- The inspector of a camera and of a volume shows the records as folds,
  each field with its override checkbox (in a volume) and the record with
  its `enabled`.
- A volume's gizmo is its shape and the blend distance as a second,
  fainter outline.
- "Create > Post-processing volume" and "Create > Shader > Post effect"
  (a template with `post.glsl`).

## Where the engine ends and the game begins

The engine gives the effects, the stack of settings with volumes and
blending, and the way to add a game's own effect as a material. Which
look a game has, when an effect fades in (a hit, a low health, a dream)
and the game's own effects are the game.

## Phases (a commit per phase)

### PP.1 The stack and the frame's needs

- Effect records with override flags, blending by the registry, the
  camera's settings, `PostProcessVolume`, the project defaults; depth
  sampling, persistent history textures, the downsample pass, `exposure()`
  from a texture.
- Tests: a field without its override flag keeps the value below it; two
  volumes blend by weight and priority; a camera outside a volume by half
  its `blendDistance` gets half its weight; switches change at half of the
  weight; the settings of a view without volumes equal the camera's; a
  history texture keeps its contents to the next frame of the same view;
  manual exposure through the texture draws the same picture as before.

### PP.2 Auto exposure and bloom

- The luminance chain, adaptation, the exposure read from the texture,
  `Auto` exposure mode; bloom.
- Tests: a scene twice as bright settles to an exposure half as large
  within `minEV` and `maxEV`; adaptation takes the time its speed gives;
  `compensation` of +1 doubles the exposure; bloom of a pixel above the
  threshold spreads light around it and a pixel below does not; turning
  an effect off removes its pass from the graph.

### PP.3 Motion vectors and anti-aliasing

- The velocity target, previous matrices in the renderer, the teleport
  mark, projection jitter, TAA, FXAA.
- Tests: a static scene with a moving camera gives motion vectors that
  match the camera's motion; a moving object's vectors match its motion;
  a teleported node gives no vector (no smear after a cut); TAA of a still
  scene converges to the average of the jittered frames.

### PP.4 Depth of field, motion blur and the look

- Depth of field, motion blur, vignette, grain, chromatic aberration,
  lens distortion.
- Tests: a point at the focus distance stays sharp and one far from it is
  blurred by the circle of confusion of the lens; motion blur of zero
  intensity changes nothing; each look effect at intensity 0 changes
  nothing (compared to the pass turned off).

### PP.5 Colour grading and custom effects

- 3D textures in the RHI, the LUT bake, `.cube` loading, `post.glsl`,
  `PostProcessEffect`, the "Post effect" shader template.
- Tests: a neutral grading gives an identity LUT; saturation 0 gives grey;
  a `.cube` file loads and applies; a custom effect that inverts colours
  runs at its point and in its order; its `intensity` blends in a
  volume and at 0 the pass is skipped.

### PP.6 Editor and documents

- Inspectors with override checkboxes, the volume gizmo, post-processing
  in the scene view, "Create" items; a "Post-processing" guide, this
  file - [done].
- Tests (logic without the interface): turning an override on copies the
  current value as the starting one; the scene view uses the main
  camera's stack.
