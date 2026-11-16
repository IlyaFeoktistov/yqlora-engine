# Cameras

[Русская версия](cameras.ru.md)

Ready camera behavior for a game: free flight, a first-person view, a
third-person camera that avoids walls, following a target, a still shot,
and smooth transitions between them. `Camera3D` (see
[rendering](rendering.md#cameras-and-rendering-done)) only looks; where it
looks is decided by the rigs of this document.

## Decisions [planned, after step 6]

- **A rig is a component for one behavior.** Flight, first person and
  third person have different settings (flight speed, the head's tilt
  limit, distance and wall avoidance), so they are different components
  with a shared base, not one camera with a list of modes where most
  fields do not belong to the chosen mode. A new behavior is a new
  component; it changes neither the base nor the other rigs.
- **A rig moves its own node.** Every frame a rig computes a pose
  (position, rotation, field of view) and puts the node it stands on into
  it. A rig on the node of the `Camera3D` itself therefore just moves the
  camera: a game with one camera needs nothing more.
- **Transitions are a `CameraDirector`.** A component on the node of a
  `Camera3D`: the camera takes the pose of the scene's enabled rig with the
  highest priority, and when the rig changes it flows smoothly from the old
  pose to the new one. Rigs on their own nodes are then shots the camera
  moves between (getting into a car, a cutscene, aiming).
- **Input is actions.** Rigs read input actions by the names in their
  properties (`LookX`, `LookY`, `MoveX`, `MoveY` by default, see
  [input](input.md#default-actions-done)), so the mouse, a stick and key
  rebinding work without code. With `inputEnabled` off only code drives
  the rig.
- **Timing.** Rigs and the director update after the components'
  `onLateUpdate`, when targets have moved for the frame and physics bodies
  are interpolated, so the camera does not lag a frame behind the
  character. Smoothing is given as a time (seconds to half the way), not a
  share per frame, so it does not depend on the frame rate. With
  `unscaledTime` a rig runs during a pause too (the camera of a pause
  menu).

## A rig: the shared part (`CameraRig`)

- `priority` (an integer; higher wins), `blendIn` (the transition to this
  rig: a time and a curve `Cut`, `Linear`, `EaseInOut`; empty means the
  director's default), `fov` (0 leaves the camera's field of view),
  `inputEnabled`, `unscaledTime`.
- `target` (a node reference) and position and rotation smoothing for the
  rigs that follow something.
- From code: all properties, `pose()` (the pose of the last frame),
  `snap()` - take the pose at once, without smoothing (after the target
  was teleported).

## Kinds of rigs

- **`FlyCamera`** - free flight: `MoveX`, `MoveY` and up and down actions
  (`Jump` and `Crouch` by default), a speed, a `Sprint` boost,
  acceleration and braking; looking as in first person. A spectator,
  debugging, level editors inside a game.
- **`FirstPersonCamera`** - a first-person view: turning by `LookX` and
  tilting by `LookY` with limits, sensitivity, `invertY`. The rig's node is
  a child of the character's head; `yawNode` is the node that turns
  horizontally with the camera (the character's body), otherwise the rig
  turns itself. From code: `yaw()`, `pitch()`, `setYawPitch`,
  `addLook(x, y)`, `forward()` (where it looks over the ground plan: a game
  moves relative to the camera by it).
- **`ThirdPersonCamera`** - an orbit around a target: a pivot above the
  target (`pivotOffset`), a distance with limits and zoom (`zoomAction`), a
  shoulder offset, tilt limits, returning behind the target after
  `recenterDelay` without input (0 never returns). **Wall avoidance**: a
  `sphereCast` from the pivot to the wanted camera place with
  `collisionRadius` by `collisionMask` (all layers but the target's by
  default); a hit brings the camera closer at once, and it moves back out
  smoothly so it does not twitch at the edge of a wall. From code:
  `orbit(yaw, pitch)`, `setDistance`, `forward()`.
- **`FollowCamera`** - following with an `offset` (in the target's or the
  world's axes), with a lag and `lookAtTarget`: a chase camera, a top-down
  view, isometric, the side view of a platformer.
- **`StaticCamera`** - stands where its node stands; with `lookAtTarget`
  it turns after the target (a security camera, a cutscene shot).
- **Shake** (after [noise](noise.md), N.1): `CameraShake` over any rig -
  kicks from code (`shake(strength, seconds)`) and a constant shake by
  noise with a frequency and a falloff.

## The director (`CameraDirector`)

- On the node of a `Camera3D`. Every frame it picks the scene's enabled
  rig with the highest `priority` (on a tie, the one enabled last) and puts
  the camera into its pose, and its field of view if the rig sets one.
- **Transition**: when the rig changes, the camera goes from the pose it
  was in to the new rig's pose over the rig's `blendIn` time (otherwise the
  director's `defaultBlend`) along the curve; the new rig keeps moving
  meanwhile, so the transition lands on a live pose, not a frozen one. A
  change in the middle of a transition starts from the current in-between
  pose.
- Position is interpolated linearly, rotation along the shortest arc
  (slerp), field of view linearly.
- From code: `activeRig()`, `isBlending()`, `cut()` (finish the
  transition at once), the event `onRigChanged` with the old and the new
  rig.
- With no enabled rig the camera stays where it was.

## Cursor capture

Looking with the mouse needs a cursor that does not stop at the window's
edge: `Input::setCursorMode` with the modes `Visible`, `Hidden` and
`Locked` (hidden and still, only its movement comes through). This is part
of [input](input.md), not of a rig: the game controls the cursor (a pause
menu gives it back). In the editor's game view the capture is released
when the view loses focus or Escape is pressed.

## Editor

- A rig has a gizmo: the view frustum from the pose the rig would take now
  (for following rigs, from the target, without input) and a line to the
  target.
- The camera preview in the corner of the scene view shows a selected rig
  too.
- In Play the game view shows the camera with the director, as in the
  game.

## Where the engine ends and the game begins

The engine gives rigs - how a camera flies, looks in first person, avoids
walls, follows and moves from shot to shot - and cursor capture. The game
decides which rig leads when (priorities, enabling and disabling), builds
cutscenes from rigs and transitions, aiming (its own rig with a smaller
`fov` and a higher priority), target lock and everything that belongs to
its rules. A ready player control built on the rigs is in
[navigation](navigation.md#player-control-planned).

## Phases (a commit per phase)

### CAM.1 Rigs and the director

- `CameraRig`, the rig system (updating after `onLateUpdate`),
  `StaticCamera`, `FollowCamera`, `CameraDirector` with priorities and
  transitions.
- Tests: a rig on the camera's node moves the camera; `FollowCamera` keeps
  its offset from a moving target and catches up with smoothing in the
  set time whatever the frame rate; the director takes the rig with the
  higher priority, and disabling a rig returns the previous one; a
  transition lasts `blendIn` and lands on a moving rig; a change in the
  middle of a transition starts from the in-between pose; `Cut` places the
  camera at once; the camera does not lag behind a character moved in
  `onUpdate`.

### CAM.2 First person, flight and the cursor

- `FirstPersonCamera`, `FlyCamera`, `Input::setCursorMode`.
- Tests: `LookX` turns the `yawNode`, `LookY` tilts the rig and stops at
  the limit; `invertY`; flight by actions with acceleration and braking;
  with `inputEnabled` off input does nothing and `addLook` works; `Locked`
  hides the cursor and gives movement; the game view releases the capture
  on losing focus.

### CAM.3 Third person

- `ThirdPersonCamera` with the orbit, zoom, shoulder, recentering and
  wall avoidance.
- Tests: a wall between the target and the camera brings the camera
  closer in the same frame, and after it the camera moves back smoothly;
  the target's layer does not get in the way; distance and tilt limits;
  recentering after `recenterDelay`; `forward()` matches the look
  direction over the plan.

### CAM.4 Editor and documents

- Rig gizmos, the rig preview, icons and descriptions; a "Cameras" guide;
  this file - [done].
- Tests (logic without the interface): a rig's pose in the edited scene is
  computed without input and without moving the node.

### CAM.5 Shake (after N.1)

- `CameraShake` with kicks and noise shake.
- Tests: a kick fades over its seconds; the same seed gives the same
  shake; shake does not accumulate in the rig's pose.
