# Animation

[Русская версия](animation.ru.md)

Animating properties of nodes and components over time: the animation
resource, the component that plays it and the editor panel with keys on a
time line.

Status marks: see [README](README.md).

## Decisions [planned]

- **Any registered property can be animated.** A track names a node and a
  property of the node or of one of its components; what can be animated
  comes only from the type registry, so a property added to a type (also a
  script field) can be animated with no extra work.
- **Three parts, each on its own**: the animation resource `.yanim` (tracks
  and keys, no scene), the `AnimationPlayer` component (which animation,
  time, speed), the Animation panel of the editor (editing). The resource
  knows nothing about the player, the player knows nothing about the panel.
- **Paths, not IDs**, inside an animation: a track names its node by the
  path from the root node of the player (`Body/Arm`), so one animation fits
  every copy of a model or a scene with the same tree.
- **Values are interpolated by their type**, never by a guess: numbers and
  vectors linearly or by a curve, colours in linear RGB, rotations as
  quaternions (shortest path), and bool, int, enum, string, node and
  resource references only step.
- **Animation runs in the update phase**, as the scene system
  `AnimationSystem` in `systems.update`, after `onUpdate` of scripts:
  scripts start, stop and seek animations in `onUpdate` and see the
  animated values in `onLateUpdate`. Physics in the fixed step sees the
  values of the previous frame.
- **Edits of an animation are in the edit history** (undo and redo) and are
  saved with Ctrl+S together with the scene; an unsaved animation is marked
  like an unsaved scene. Keys are edited often and many at once, so undo
  is needed from the start.
- **Not in the first version**: blending and cross fading between
  animations, a state machine, skeletal animation and skinning, animations
  from glTF, animating values inside a material. Each of them comes as an
  addition, without changing the format of `.yanim` (see "Later").

## Animation resource

A `.yanim` file (JSON, `FileFormat` version 1):

```json
{
    "length": 2.0,
    "loop": "loop",
    "tracks": [
        {
            "node": "Body/Arm",
            "component": "",
            "property": "rotationDegrees",
            "keys": [
                {"time": 0.0, "value": [0, 0, 0], "interpolation": "linear"},
                {"time": 1.0, "value": [0, 90, 0], "interpolation": "cubic",
                 "in": [-0.3, 0.0], "out": [0.3, 0.0]},
                {"time": 2.0, "value": [0, 0, 0], "interpolation": "linear"}
            ]
        },
        {
            "node": "Body",
            "component": "MeshRenderer",
            "property": "visible",
            "keys": [{"time": 1.5, "value": false}]
        }
    ],
    "events": [{"time": 1.0, "node": "", "component": "Footsteps", "method": "step"}],
    "version": 1
}
```

- `length` in seconds; `loop`: `none` (stops at the end and keeps the last
  values), `loop`, `pingPong`.
- A track: `node` (a path from the root of the player, empty for the root
  itself), `component` (the type name, empty for a property of the node;
  the first component of that type, `#n` for another one: `Light3D#1`),
  `property` (its registered name), `keys` sorted by time.
- A key: `time`, `value` (the same JSON as the property in a scene file),
  `interpolation` of the segment after it: `constant`, `linear`, `cubic`
  (`in` and `out` are the tangent handles, time and value offsets, for
  numbers; vectors and colours have one handle pair per key, for the whole
  value, so the curve is an easing curve).
- Events call a registered method without arguments on a component at a
  time; an event is called once per pass over its time, also when a frame
  jumps over it.
- A property renamed with `formerly` is found by its old name, and the new
  one is written, as in scene files. A track whose node, component or
  property is not found is skipped with one message in the log and kept in
  the file unchanged.
- The type of the values of a track is checked against the property on
  load: a key of another type is an error of the track, not converted.

## Playing

```cpp
auto& player = node().component<AnimationPlayer>();
player.play(walk);              // a resource reference
player.setSpeed(1.5f);
player.seek(0.5f);
if (!player.isPlaying()) { ... }
player.stop();                  // to the start, values stay as they are
```

- `AnimationPlayer` properties: `animations` (a list of `.yanim`
  references), `autoplay` (one of them, started in `onStart`), `speed`
  (negative plays backwards), `root` (a node reference, the owner when
  empty). Methods are registered, so scripts in every language and the
  visual palette get them.
- `onFinished` is an event of the player for scripts (`none` loop only).
- While no animation plays, the player writes nothing: properties keep the
  values set by scripts or the scene.
- Resolving paths to nodes and properties is done when an animation starts
  and again after a change of the tree (`Scene` tells the player through
  the deferred tree changes), not every frame.
- A reload of the resource (a new revision) resolves the tracks again and
  keeps the time.

## Editor: the Animation panel

A panel at the bottom, a tab next to Files and Log.

```
[AnimationPlayer: Hero v] [walk v] [|<] [>] [>|]  0.50 / 2.00 s  [Loop v] [* Rec]
-----------------------------------------------------------------------------
 Tracks                     | 0.0      0.5      1.0      1.5      2.0
 Body/Arm  rotationDegrees  |  <>                <>                <>
 Body  MeshRenderer.visible |                            <>
 Events                     |                  |
```

- The panel edits the animations of the `AnimationPlayer` selected in the
  tree (or of the player above the selected node); the second list chooses
  the animation, "New animation" creates a `.yanim` through the creation
  window.
- **Time line**: a ruler with zoom (wheel) and scroll, the current time as
  a line dragged with the mouse, snapping of time to steps (0.1 s by
  default, off with Shift), the length dragged at the end of the ruler.
- **Keys**: diamonds on the rows of tracks; select by click, Ctrl+click and
  a frame; drag in time; Delete; copy and paste at the current time;
  the interpolation of the selected keys in the context menu.
- **Adding keys**: a key button next to every property in the inspector
  ("Add key at the current time", a filled button when the property has a
  key at that time); the record mode (Rec): every change of a property in
  the inspector or by a manipulator adds or changes a key of its track,
  creating the track when there is none.
- **Preview**: dragging the time or pressing play applies the animation to
  the edited scene. The values the animation changes are kept before the
  first preview and written back when the preview stops, the panel closes,
  the scene is saved or compared for "unsaved", so a preview never makes the
  scene changed and never gets into the file.
- **Curves** of a track (the value over time with the cubic handles) are a
  second view of the panel, switched by a button; the first version opens
  them for float tracks and the components of vectors.
- The logic (selection of keys, commands, snapping, preview with restoring)
  lives in `editor/src` without ImGui; the panel in `editor/ui` only draws
  it.

## Later

- Animations from glTF as nested resources of a model (`animation:Name`)
  with tracks of node transforms, then skinning (bones, weights in
  `Vertex`, skinned meshes) and morph targets.
- Blending and cross fading (`player.crossFade(run, 0.3f)`), layers, a state
  machine resource with transitions by parameters.
- Tracks of material values through `MaterialOverrides`, tracks of whole
  resources (switching a mesh, a texture), audio events after step 7.

## Phases (a commit per phase)

### A.1 Resource and interpolation

- `AnimationData` (tracks, keys, events), the `.yanim` loader and writer,
  the resource kind.
- Interpolation by `PropertyValue` type: constant, linear, cubic, colours in
  linear RGB, rotations through quaternions.
- **Tests**: linear and cubic values between keys; step for bool, enum and
  references; a rotation from 350 to 10 degrees goes through 0, not 180; a
  key of a wrong type is an error of the track; the file goes through JSON
  and back keeping unknown tracks.

### A.2 Playing

- `AnimationPlayer`, `AnimationSystem` in `systems.update`, resolving
  paths, `none`, `loop`, `pingPong`, negative speed, events, `onFinished`.
- **Tests**: after `advance` the property has the value of the time; a
  script sees animated values in `onLateUpdate`; a renamed node skips its
  track with one message; an event is called once when a frame jumps over
  it; `stop` keeps the values; a new revision keeps the time.

### A.3 Animation panel

- The panel, the time line, keys and their commands in the history,
  the key button in the inspector, Rec, preview with restoring.
- **Tests** (editor logic): adding, moving and deleting keys are undone;
  Rec adds a key when a property changes; after the preview the scene is not
  changed; snapping of time.

### A.4 Curves and documents

- The curves view; documents: this file (marks [done]), scene (the
  component), project (the format), editor (the panel), the guide
  "Animating a door", README.
