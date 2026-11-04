# Particles

[Русская версия](particles.ru.md)

Fire, smoke, sparks, dust, rain: many small short lived objects that a
`ParticleEmitter` creates, moves and draws.

Status marks: see [README](README.md).

## Decisions [planned]

- **Simulated on the CPU, drawn with instancing.** The RHI has no compute
  shaders on OpenGL 3.3, so the first version moves particles in C++
  (arrays by field, no object per particle) and draws all particles of an
  emitter in one instanced draw. A GPU simulation comes with a backend that
  has compute shaders, behind the same component and resource.
- **Three parts, each on its own**: the effect resource `.yparticles`
  (what an emitter does: shape, rates, curves, look), the `ParticleEmitter`
  component (which effect, playing, its own particles), the drawing in the
  render graph. The resource knows nothing about the scene, the emitter
  knows nothing about the GPU.
- **Behaviour is a list of modules** in the resource, each with its own
  fields, as records in the registry: a new module is a new record and a
  function, nothing else changes. The inspector shows the list with
  checkboxes and folds.
- **Values that change over a lifetime are curves and gradients**, one type
  each (`Curve`, `Gradient`), shared with animation and noise later; a value
  can be a constant, a random range or a curve.
- **Particles are drawn by materials.** A particle shader is an ordinary
  `.yshader` with `#include <yqlora/particles.glsl>`: it gets the instance
  data (position, size, rotation, colour, age) and builds a quad facing the
  camera. Built-in `Particle Unlit` (additive or alpha) and `Particle Lit`.
  Transparent particles are sorted back to front per emitter.
- **Play and editor**: an emitter plays in Play and, when selected, in the
  editor viewport (preview with restart, pause and speed buttons), without
  changing the scene.
- **Deterministic by seed**: an emitter with a fixed `seed` gives the same
  particles on every run with the same steps, so tests can check them.

## Effect resource

A `.yparticles` file (JSON, `FileFormat` version 1):

```json
{
    "duration": 2.0,
    "loop": true,
    "maxParticles": 1000,
    "space": "world",
    "modules": [
        {"type": "Emission", "rate": 50, "bursts": [{"time": 0.0, "count": 20}]},
        {"type": "Shape", "shape": "Cone", "angle": 25, "radius": 0.2},
        {"type": "Start", "lifetime": [1.0, 2.0], "speed": [2.0, 4.0], "size": 0.2,
         "color": [1, 0.8, 0.4, 1]},
        {"type": "Gravity", "scale": 0.3},
        {"type": "ColorOverLifetime", "gradient": {"keys": [[0, [1, 1, 1, 1]], [1, [1, 0.2, 0, 0]]]}},
        {"type": "SizeOverLifetime", "curve": {"keys": [[0, 0.5], [1, 1.5]]}},
        {"type": "Noise", "noise": "4c1e07a95b3d2f60", "strength": 0.5}
    ],
    "material": "9bf87a121dc2ad5c",
    "render": "Billboard",
    "version": 1
}
```

- Modules of the first version: `Emission` (rate per second and per metre
  moved, bursts), `Shape` (point, sphere, hemisphere, cone, box, circle,
  edge, a mesh surface), `Start` (lifetime, speed, size, rotation, colour),
  `Gravity`, `Drag`, `VelocityOverLifetime`, `ColorOverLifetime`,
  `SizeOverLifetime`, `RotationOverLifetime`, `Noise` (turbulence from a
  [noise](noise.md) resource), `Collision` (with planes now, with the
  physics world after step 6), `SubEmitter` (another effect on birth or
  death of a particle), `SpriteSheet` (frames of a texture over the
  lifetime).
- `space`: `world` (particles stay where they were born when the emitter
  moves) or `local` (they move with the node).
- `render`: `Billboard` (faces the camera), `StretchedBillboard` (along
  the velocity), `HorizontalBillboard`, `Mesh` (an instanced mesh per
  particle).
- A module whose type is unknown is kept in the file unchanged and skipped
  with one message, as unknown tracks of an animation.

## Emitter

- `ParticleEmitter` properties: `effect`, `playOnStart`, `seed` (0 means a
  new seed each play), `simulationSpeed`, `prewarm` (a looping effect
  starts as if it had played one `duration`).
- Methods: `play`, `stop` (no new particles, the living ones finish),
  `clear`, `emit(count)`, `isAlive`; registered, so scripts in every
  language get them.
- Simulation runs in `systems.update` (`ParticleSystem`), after `onUpdate`
  of scripts, on game time: pause and time scale apply.
- Bounds of the living particles are kept per emitter for culling and
  picking in the editor.

## Drawing

- RHI: instanced draws (`drawInstanced`, `drawIndexedInstanced`) and
  per-instance vertex inputs in the pipeline description; the OpenGL
  backend (`glDrawArraysInstanced`, `glVertexAttribDivisor`).
- Extract copies the instance data of every visible emitter into a frame
  buffer (`UniformRing` sized for streams); Prepare draws them with the
  transparent items, sorted by emitter distance and inside an emitter by
  particle distance.
- Soft particles (fading near opaque surfaces) need the depth of the scene
  as a texture; they come with post-processing, after the MVP.

## Editor

- "Create resource > Particle effect" in Files; the inspector of the effect
  shows the modules, a module menu to add one, and edits go straight into
  the file, as with materials.
- An emitter selected in the scene plays in the viewport with a small
  panel: restart, pause, speed, particle count. Its bounds are drawn as a
  gizmo.
- Curve and gradient fields are widgets of their own in `editor/ui`, the
  logic of editing keys in `editor/src`; the animation panel uses the same
  curve logic.

## Phases (a commit per phase)

### P.1 Curves, gradients, instancing

- `Curve`, `Gradient` as records with evaluation; RHI instancing and its
  OpenGL backend; the test device records instanced draws.
- **Tests**: curve values between keys; gradient colours in linear RGB;
  on OpenGL: 100 instances of a quad give 100 squares at their offsets.

### P.2 Simulation

- `.yparticles` loader and writer, modules of the first version (without
  `Collision` with physics and `SubEmitter`), `ParticleEmitter`,
  `ParticleSystem`, seed, prewarm.
- **Tests**: the same seed gives the same particles; `maxParticles` is
  never exceeded; a burst of 20 gives 20; particles die after their
  lifetime; `world` space keeps particles when the node moves, `local`
  moves them; a paused game moves nothing.

### P.3 Drawing

- `<yqlora/particles.glsl>`, `Particle Unlit` and `Particle Lit`, instance
  data in Extract, sorting, the four render modes.
- **Tests**: on OpenGL an emitter with one particle draws one quad facing
  the camera; additive particles add up; alpha particles are drawn back to
  front.

### P.4 Editor, sub emitters, documents

- Creating effects, the inspector with modules, curve and gradient fields,
  preview in the viewport; `SubEmitter`, `Collision` with planes.
- Documents: this file (marks [done]), rendering (instancing, the
  particle pass), project (the format), the guide "Fire and smoke", README.
