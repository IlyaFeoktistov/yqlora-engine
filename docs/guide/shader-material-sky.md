# Your own shader, material and sky

[Русская версия](shader-material-sky.ru.md)

A material says how a surface looks: which shader draws it and with which
values and textures. A shader is a GLSL file of the project. The sky is a
material too, with a sky shader. All of them are project files, made in
Files and kept in git with their `.meta`.

## 1. A material

In Files: Create resource > Material, give it a name, Create. A new
material uses the built-in `Standard` shader and is white.

Click it: the inspector shows a preview sphere, the shader, the render
state (blending, cull mode) and the fields of the shader: base colour,
metallic, roughness, emission and the maps. A change is seen at once on
every object with the material and is written into the file when you let
go of the slider. Material edits are not in the edit history of the scene.

Put it on an object:

- drag the material into the viewport onto the object: it goes into the
  slot of the part under the cursor;
- or choose it in a slot of the `MeshRenderer` in the inspector. A mesh has
  one slot per submesh; an empty slot draws with the default material.

Assigning a material is a scene edit: Ctrl+Z takes it back.

A model brings its own materials and textures: they are under the model in
Files, and a model dropped into the scene gets them in its slots. They come
from the file and are read-only; press "Make a copy" in the inspector to get
a `.ymat` next to the model that you can change.

## 2. A shader

Create resource > Shader makes a lit shader from a template: a colour, a
texture, metallic and roughness. Double-click it in Files (or press Open
in its inspector) to edit it in the program your system opens `.yshader`
with.

```glsl
#version 450
#include <yqlora/standard.glsl>

layout(set = 2, binding = 0, std140) uniform Material {
    vec4 baseColor;  //@ color default(1, 1, 1, 1)
    float roughness; //@ range(0, 1) default(0.5)
} material;

layout(set = 2, binding = 1) uniform sampler2D baseColorMap; //@ default(white)

#ifdef YQLORA_VERTEX
void main()
{
    standardVertex();
}
#endif

#ifdef YQLORA_FRAGMENT
void main()
{
    vec4 color = material.baseColor * texture(baseColorMap, vUv0);
    Surface surface = defaultSurface();
    surface.baseColor = color.rgb;
    surface.alpha = color.a;
    surface.roughness = material.roughness;
    fragColor = shade(surface);
}
#endif
```

- One file holds both stages: the engine compiles it with `YQLORA_VERTEX`
  and with `YQLORA_FRAGMENT`.
- The `Material` block (set 2, binding 0) and the textures of set 2 become
  the fields of every material with this shader. The `//@` comments say how
  the inspector shows them: `color`, `range(min, max)`, `default(...)`,
  `label("...")`, `hidden`.
- `shade(surface)` lights the surface with every light of the scene, their
  shadows, the light of the sky and its reflections. A shader that sets
  only `baseColor` still gets all of that.

Save the file and go back to the editor: it compiles changed shaders when
its window gets focus, and every material with the shader updates. An
error goes to the Log panel as `file:line: text`; a click on it opens the
file at that line. Until the shader compiles, its materials draw in
magenta (`Error`).

To use the shader, choose it in the Shader field of a material.

## 3. A sky

The sky of a scene is the `sky` field of its `Environment` node (a new scene
has one). The built-in `Default Sky` is the physical sky of the atmosphere:
it follows the sun.

Your own sky: Create resource > Sky Shader, then a material with that
shader, then put the material into the `sky` field of `Environment`. A sky
shader defines one function:

```glsl
#include <yqlora/sky.glsl>

vec3 sky(vec3 direction)
{
    // luminance of the sky in nits in this direction
}
```

The template draws a colour from the horizon to the zenith, the disc of the
sun and a glow around it. The sky also lights the scene: the ambient light
and the reflections come from it (`ambientSource` of `Environment`: `Sky`,
`Gradient` or `Color`).

## 4. Light

- The sun is a `DirectionalLight3D` with the `sun` flag; its rotation is the
  direction of the light and the time of day of the physical sky.
- `PointLight3D` and `SpotLight3D` light around them up to their range.
- Units are physical: the sun in lux (100000 by default), lamps in candela
  (130, a 100 W bulb). The camera exposure (`exposure` of `Camera3D`, EV100)
  says how bright the picture is: 15 for a sunny day, lower for a room or
  a night.
- Every light has its shadows: off, hard or soft; `castShadows` and
  `receiveShadows` of a `MeshRenderer` turn them off for one object.
  Project settings > Graphics hold the shadow settings of the whole project.

## If something is wrong

- **The object is magenta**: its shader does not compile (see the Log), or a
  sky material is in a mesh slot.
- **No sky**: the scene has no `Environment`, its `sky` field is empty, or
  the material in it does not have a sky shader (the Log says so).
- **Everything is white or black**: the exposure of the camera does not fit
  the light; a lamp at night with EV100 15 is almost black.
- **A material does not change**: it belongs to a model; make a copy.
