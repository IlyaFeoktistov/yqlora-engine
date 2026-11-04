# Noise

[Русская версия](noise.ru.md)

Procedural noise for the whole engine: in C++ and scripts, as a texture for
materials and as functions in shaders. One set of algorithms gives the same
values everywhere.

Status marks: see [README](README.md).

## Decisions [planned, after step 5]

- **One source of algorithms**: [FastNoiseLite](https://github.com/Auburn/FastNoiseLite)
  (MIT), pinned to a commit like the other libraries. It ships the same
  algorithms as a C++ header and as a GLSL file, so a noise built in C++ and
  the same noise computed in a shader agree (up to float precision). The
  engine does not write its own copies of the algorithms.
- **Settings are one record**, `NoiseSettings`, registered in the type
  registry. The C++ class, a script field, the noise resource and the noise
  texture all hold this record, so the inspector, the files and the
  scripts see one list of fields.
- **Three levels, not mixed**: `Noise` (values at points, no files, no GPU),
  the noise resource `.ynoise` (settings in a file, shared by scripts and
  textures), the noise texture `.ynoisetex` (an image made from a noise, a
  texture like any other for materials).
- **Values are in [-1, 1]** for every kind; a texture maps them to [0, 1]
  (or to [min, max] after normalizing).

## Settings

`NoiseSettings` (a record, every field with a default and a range):

- `kind`: `OpenSimplex2`, `OpenSimplex2S`, `Cellular`, `Perlin`,
  `ValueCubic`, `Value`;
- `seed` (int), `frequency` (float), `offset` (vec3, moves the noise in
  space without changing its shape);
- fractal: `fractal` (`None`, `FBm`, `Ridged`, `PingPong`), `octaves`,
  `lacunarity`, `gain`, `weightedStrength`, `pingPongStrength`;
- cellular: `cellularDistance` (`Euclidean`, `EuclideanSq`, `Manhattan`,
  `Hybrid`), `cellularReturn` (`CellValue`, `Distance`, `Distance2`,
  `Distance2Add`, `Distance2Sub`, `Distance2Mul`, `Distance2Div`),
  `cellularJitter`;
- domain warp: `warp` (`None`, `OpenSimplex2`, `OpenSimplex2Reduced`,
  `BasicGrid`), `warpAmplitude`, `warpFrequency`, `warpFractal` (`None`,
  `Progressive`, `Independent`), `warpOctaves`, `warpLacunarity`,
  `warpGain`.

Enum values are written by name (`YQLORA_ENUM_NAMES`), so the files stay
readable and the C++ order can change.

## Noise in code

```cpp
yqlora::Noise noise({.kind = NoiseKind::Perlin, .seed = 7, .frequency = 0.05f});
float height = noise.sample(x, z);          // 2D
float density = noise.sample(position);     // 3D, Vec3
Vec3 warped = noise.warp(position);         // domain warp alone
```

- `Noise` (`engine/include/yqlora/math/Noise.hpp`) is a value: settings in,
  numbers out. `sample` is `const` and safe to call from several threads.
- `setSettings` replaces all settings at once; there are no setters per
  field that would leave the object half changed.
- A script takes a noise from a resource (`resource.noise()`) or keeps its
  own `NoiseSettings` field, shown in the inspector like any record.
- Luau and visual scripts get `Noise` through the bindings generated from
  the registry (its methods are registered), with nothing written by hand.

## Noise resource

A `.ynoise` file (JSON, `FileFormat` version 1) holds one `NoiseSettings`:

```json
{
    "kind": "Perlin",
    "seed": 7,
    "frequency": 0.05,
    "fractal": "FBm",
    "octaves": 4,
    "version": 1
}
```

- Created in Files ("Create resource > Noise") through the creation window,
  lies anywhere in the project.
- A field missing in the file takes its default, a field the record no
  longer has is kept in the file and written back (as with materials).
- A change raises the revision; noise textures that use it are made again.

## Noise texture

A `.ynoisetex` file is a texture made from a noise. Material slots take it
like a `.png`: it has the texture kind, and `ResourceCache` keeps its
`TextureData`.

```json
{
    "noise": "4c1e07a95b3d2f60",
    "width": 512,
    "height": 512,
    "dimensions": "2D",
    "seamless": true,
    "seamlessBlend": 0.1,
    "invert": false,
    "normalize": true,
    "output": "Color",
    "bumpStrength": 8.0,
    "colorSpace": "Linear",
    "version": 1
}
```

- `noise` is a reference to a `.ynoise`, or the file holds its own
  `NoiseSettings` under `settings` when no shared noise is wanted; never
  both.
- `seamless`: the image tiles without a seam. FastNoiseLite has no 4D
  noise to sample on a torus, so the image is made larger and its edges
  are blended with the opposite ones over the `seamlessBlend` part of the
  size (0 to 1).
- `normalize`: the values of the image are stretched to [0, 1] by their
  real minimum and maximum.
- `output`: `Color` (grey, one value in RGB) or `NormalMap` (a tangent
  space normal map from the height, strength `bumpStrength`, +Y up as the
  `normalMap` of `Standard` expects).
- `dimensions`: `2D` now; `3D` (a volume texture) waits for 3D textures in
  the RHI.
- Making the image is synchronous, rows split between threads; the size is
  limited to 4096 x 4096. Background making comes with background loading
  of resources.
- A gradient (colour ramp) on the output is a separate resource later; the
  file gets a `gradient` reference then, without a new format version.

## In shaders

```glsl
#include <yqlora/noise.glsl>

fnl_state state = fnlCreateState(7);
state.noise_type = FNL_NOISE_PERLIN;
state.frequency = 0.05;
float value = fnlGetNoise3D(state, position.x, position.y, position.z);
```

- `<yqlora/noise.glsl>` is the GLSL file of FastNoiseLite, unchanged,
  shipped with the engine library of shaders. The names are its own, so
  its documentation applies as is.
- A material does not pass a noise resource to the shader: a shader keeps
  its parameters in the `Material` block like any other value, or samples
  a noise texture.

## Editor

- The inspector of a `.ynoise` and a `.ynoisetex` shows the fields of the
  record and a preview image (256 x 256, made again after an edit, at most
  once per frame); edits go straight into the file, as with materials.
- Files shows a noise texture with its image as the icon.

## Phases (a commit per phase)

### N.1 Noise in code

- FastNoiseLite added (C++ and GLSL files, one commit pinned), its licence
  in the third party list.
- `NoiseSettings` with its enums and names, registered as a record;
  `Noise` with `sample` 2D and 3D and `warp`.
- **Tests**: the same settings and seed give the same values twice; each
  kind gives values in [-1, 1] on a grid of points; a changed seed changes
  the values; `offset` moves the noise (`sample(p + offset)` of one equals
  `sample(p)` of the other); `NoiseSettings` goes through JSON and back.

### N.2 Noise resource and noise texture

- `.ynoise` and `.ynoisetex` loaders, resource kinds, revisions and the
  dependency of a texture on its noise (a new noise revision makes the
  texture again).
- Making the image: `Color`, `NormalMap`, `seamless`, `invert`,
  `normalize`.
- **Tests**: a noise texture has the given size; `seamless` gives equal
  values on opposite edges; `normalize` gives a minimum 0 and a maximum 1;
  `NormalMap` of a flat noise is (0.5, 0.5, 1); a changed `.ynoise` makes
  a new texture revision; a material takes a noise texture in a slot.

### N.3 Noise in shaders

- `<yqlora/noise.glsl>` in the engine library, compiled by the same
  compiler and profile.
- **Tests**: a shader with `noise.glsl` compiles; on OpenGL a full screen
  shader that writes `fnlGetNoise2D` gives the values of `Noise` at the same
  points within 1e-3.

### N.4 Editor and documents

- Creating both resources in Files, inspectors with the preview, icons.
- Documents: this file (marks [done]), resources, project (the two
  formats), the guide "Noise in scripts, materials and shaders", README.
