# Wind

[Русская версия](wind.ru.md)

One wind for the whole scene: grass, trees, cloth and particles sway
together, from the same zones and the same gusts. Wind is a field of
velocity in the world; whoever needs it reads it.

## Decisions [planned, after the MVP, after noise]

- **One field, many readers.** `WindSystem` (a scene system) knows the
  wind zones and answers `velocityAt(position)` in metres per second.
  Cloth, particles, vegetation and the game read it; none of them has its
  own wind settings. A new reader adds nothing to the system.
- **Zones are components.** `WindZone` on a `Node3D`: the shape of the
  zone, a base speed along the node's forward axis, turbulence and gusts.
  The scene's wind at a point is the sum of the zones that cover it,
  each weighted by its falloff. Without zones there is no wind.
- **Gusts and turbulence are noise.** Gusts are a slow 1D noise over time
  (the strength rises and falls), turbulence a 3D noise over space and
  time (the direction wobbles from place to place), both from
  [noise](noise.md) (N.1 in code, N.3 in shaders), so the CPU and the GPU
  compute the same wind from the same seed.
- **The GPU gets the same field.** Each view carries up to
  `kMaxWindZones` zones nearest to its camera (the global ones first) in
  the `View` block, so the scene view and the game view sway alike and the
  shadow passes of a view bend vegetation as its colour pass does.
  `#include <yqlora/wind.glsl>` gives `windVelocity(worldPosition)` and
  `windBend(...)`. The formula is written twice, in GLSL and in C++, and a
  test on OpenGL draws `windVelocity` at a hundred points into a float
  target, reads it back and compares it with `velocityAt`.
- **Time in shaders.** The `Frame` block gets `time` (game time since the
  scene started: it stops on pause and follows the time scale) and
  `unscaledTime`, as floats; the wind and every animated material use
  them. They are not wrapped: a wrap would be a jump in every animated
  material, while a float keeps steps under 1 ms for two hours and under
  8 ms for a day, below one frame.

## Wind zone (`WindZone`)

- `shape`: `Global` (everywhere, no position), `Sphere` (`radius`), `Box`
  (`size`); `falloff` - the width of the edge where the zone fades out.
- `speed` (m/s) along the node's forward axis (`-Z`); a `Sphere` zone with
  `radial` blows outward from its centre instead (an explosion, a
  helicopter's downwash; a negative speed pulls in).
- `turbulence` (0 to 1: the share of the speed the direction wobbles by)
  and `turbulenceScale` (metres per wobble).
- `gustStrength` (a share of the speed added at the peak of a gust),
  `gustFrequency` (gusts per minute on average), `gustScale` (the size of
  a gust in metres). A gust moves downwind with the zone's speed, so
  neighbouring trees bend one after another, not all at once.
- `seed`.
- A zone's values change on the fly and act from the next frame.

## Readers

- **Vegetation** (see [terrain](terrain.md)): sways in the vertex shader
  by `windBend` with the stiffness of the plant (from vertex colours or a
  material parameter): trunk, branches and leaves bend at their own rate.
- **Cloth** (see [cloth](cloth.md)): a force on each face by the wind
  velocity relative to the face, along its normal.
- **Particles** (see [particles](particles.md)): a `Wind` module pulls the
  particle's velocity toward the wind's with the rate `influence` (per
  second), as air drag does.
- **Rigid bodies** are not blown by default: wind on a crate is a rule of
  a game. A game reads `velocityAt` and applies a force (`addForce`).

## From code

`wind().velocityAt(position)` from a script; all properties of
`WindZone`. A game makes a storm by raising the `speed` of the global zone
over time, and calm by turning the zones off.

## Editor

- A zone's gizmo: its shape and an arrow of its direction with the length
  of its speed.
- "Wind" channel of the "Gizmos" menu (`wind.field`): arrows of the wind on
  a grid around the camera, live in Play and in the edited scene, so a
  zone is tuned while looking at the field.
- The edited scene runs `time` for materials, so grass sways in the
  editor too; "Animate materials" in the view's menu stops it (for
  screenshots and for comparing).

## Where the engine ends and the game begins

The engine gives the wind field, its zones and gusts, the same wind on the
CPU and the GPU, and the engine's readers: vegetation, cloth, particles.
Whether wind pushes bodies, slows a character or carries a sailboat is the
game, reading the same field.

## Phases (a commit per phase)

### WD.1 Time in shaders and the wind field

- `time` and `unscaledTime` in the `Frame` block; `WindZone`,
  `WindSystem` with `velocityAt`, gusts and turbulence by noise; the zone
  gizmo and the field channel.
- Tests: without zones the wind is zero; a global zone gives its speed
  along the node's forward axis; a sphere zone fades to zero over its
  `falloff`; two zones add; `radial` blows away from the centre; gusts
  with the same seed give the same strength over time and stay within
  `gustStrength`; a point downwind gets the same gust later by its
  distance over the speed; on pause the gusts stop.

### WD.2 Wind on the GPU

- The nearest zones in the `View` block, `wind.glsl` with `windVelocity`
  and `windBend`, time in the edited scene and "Animate materials".
- Tests: on OpenGL, `windVelocity` drawn at a hundred points and read back
  equals `velocityAt` within tolerance; more zones than `kMaxWindZones`
  keep the global ones and the nearest to the view's camera; a material
  with `windBend` compiles with the standard library; with "Animate
  materials" off `time` of the edited scene stays still.

### WD.3 Readers and documents

- The `Wind` particle module, a "Wind" guide, this file - [done]. The
  wind force of cloth is in [CL.2](cloth.md#cl2-wind-air-and-the-budget).
- Tests: a still particle with `influence` 1 in a 5 m/s wind approaches
  the wind's velocity as `1 - e^-t`; a particle with `influence` 0 does not
  feel it.
