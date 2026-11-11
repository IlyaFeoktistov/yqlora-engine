# Physics

[Русская версия](physics.ru.md)

Step 6 of the MVP: rigid bodies, colliders, contacts and triggers, queries,
a character controller and joints on
[Jolt Physics](https://github.com/jrouwe/JoltPhysics) (MIT).

Status marks: see [README](README.md).

## Decisions [planned, step 6]

The design is the established one for a component physics engine, and Jolt
is wired as its documentation and samples recommend; nothing here is new.

- **Jolt, a pinned release** (v5.6.0), built by CMake with the engine and
  linked privately: no public engine header names a Jolt type, so its build
  defines (precision, debug renderer, processor features) never have to
  match the code of a game. Single precision: worlds up to about 10 km;
  double precision is a define away when large worlds come. Processor
  features stop at SSE4.2, so a game runs on any x86-64 from 2008 on, not
  only where AVX2 is. `JPH::VerifyJoltVersionID` checks at start that the
  engine and the Jolt library agree.
- **`PhysicsWorld`**, a scene system with a public interface without Jolt
  (the Jolt objects are behind a pointer to its implementation), owns one
  `JPH::PhysicsSystem` per scene: the edited scene and its Play copy have
  their own. It is created when the first body comes, so a scene without
  physics, and every test without it, pays nothing.
- **One Jolt runtime per process**: the job system (`JobSystemThreadPool`,
  cores minus one) and the temporary allocator are shared by all worlds of
  the services (`EngineServices::physics()`, `PhysicsRuntime`), created on
  first use. The global setup of Jolt (allocator hooks, `Factory`, the
  registered types, its trace and assert going to our log) is made by the
  first runtime and undone by the last one, counted, never hidden in a
  static initializer.
- **Stepping**: Jolt steps in a new stage of the frame,
  `systems.afterFixedUpdate`, after `onFixedUpdate` of components: a force a
  script adds in `onFixedUpdate` acts in the same step, and the contact
  events of the step come right after it, before the next fixed step or
  `onUpdate`. One collision step per fixed step of
  `TimeSettings::fixedStep`; the frame catch-up limit of `GameTime` also
  limits physics. Only a running scene (the Play copy, a game) is stepped;
  the edited scene keeps its bodies for queries and gizmos.
- **Transforms**: physics computes the world pose of a node itself from
  its local transforms (world matrices of the frame are updated only after
  the simulation, so during fixed steps they may be old). Every body keeps
  the pose it last wrote into its node. Before a step, a node whose pose
  differs from that is taken as moved by code or the editor: a static or
  dynamic body teleports, a kinematic one gets a target (`MoveKinematic`,
  so a platform carries what stands on it). After a step a dynamic body
  writes its pose into the node (world to local through the parent) and
  remembers it. **Interpolation** (`RigidBody::interpolation`) writes a
  pose between the last two steps in `update`, and that pose is what the
  body remembers, so a smoothed node is never taken for a moved one; the
  physics state itself is not changed by it. A change of scale rebuilds
  the shape.
- **Layers**: the 32 node layers of the project are what collides with
  what; the collision matrix (32 by 32, symmetric) is in the Physics
  section of the project settings, edited as a table of the named layers.
  A Jolt object layer is a node layer and the motion (static or moving),
  64 in all, since Jolt takes the broad phase layer from the object layer:
  two broad phase layers, static and moving, as Jolt advises. Static never
  collides with static. Each step takes a copy of the matrix, so the job
  threads never read the settings while the editor changes them.
- **Gravity** and the solver iterations are in the Physics section. A world
  starts with the gravity of the settings; `setGravity` from a script
  changes it until the scene ends; a change of the settings during Play is
  applied at once (the editor), also over a script's value.
- **Units**: metres, kilograms, seconds; gravity -9.81 along Y by default.
- **Contacts are reported on the main thread**: Jolt calls its listener
  on worker threads, so the listener only records pairs into a buffer, and
  `PhysicsWorld` emits the events after the step, in a stable order.
- **Bodies and the scene tree**: a body belongs to its component and is
  added in `onEnterScene` and removed in `onExitScene`; the body keeps the
  `ComponentId` of its component as Jolt user data, so a contact or a ray
  gives a node by an index, never a dangling pointer. A node destroyed
  during a frame keeps its body until the end of the frame, as the node
  itself lives that long.
- **Run-time state is not saved**: velocities and contacts live in Jolt;
  a hot reload and the Play copy start from the properties, as for any
  component.

## Bodies and colliders [planned, step 6]

- **`RigidBody`** (on a `Node3D`): `motion` `Static`, `Kinematic` or
  `Dynamic` (the Jolt motion type), `mass` (or from density and volume),
  `linearDamping`, `angularDamping`, `gravityScale`, `lockPosition` and
  `lockRotation` per axis (Jolt allowed degrees of freedom),
  `continuousCollision` (Jolt `LinearCast` motion quality, for fast small
  bodies), `interpolation`, `startAsleep`. A body with only static motion
  never moves; one that must be moved by code is kinematic.
- **Colliders** are components: `BoxCollider`, `SphereCollider`,
  `CapsuleCollider`, `CylinderCollider`, `ConvexCollider` (a hull of a
  mesh), `MeshCollider` (the triangles of a mesh; static and kinematic
  bodies only, as Jolt requires). Each has `center` (and rotation where
  it matters), its size, `isTrigger` and `material`.
- **A body is made of the colliders of its node and its descendants**, up to
  the next node with its own `RigidBody`: several colliders form one
  compound shape. A collider with no `RigidBody` above it makes a static
  body of its own, so a wall needs only a collider.
- **Scale** of nodes reaches the shapes: non-uniform scale where Jolt
  allows it (box, convex, mesh, as `ScaledShape`); a sphere and a capsule
  take the largest axis and the log says so once.
- **Shapes of meshes are cooked once** per mesh and revision and shared by
  all colliders with that mesh; a re-export rebuilds them.
- **`PhysicsMaterial`**, a resource (`.ymat` is taken by render materials,
  so `.yphysmat`): `friction`, `restitution` and how two values combine
  (average, minimum, maximum, multiply). Per collider, combined in the
  contact listener (`OnContactAdded`, since Jolt has one combine function
  per world); when the two modes differ, the higher one in maximum,
  multiply, minimum, average is used, so the result does not depend on
  which body is first. Without a material the project default is used.

## Contacts and triggers [planned, step 6]

- **Events** (see [events](events.md)) on `RigidBody`: `onCollisionEnter`,
  `onCollisionStay`, `onCollisionExit`, with a `Collision` record (the
  other node and collider, contact points and normals, the relative
  velocity, the impulse). On a trigger collider: `onTriggerEnter`,
  `onTriggerExit` with the other node. Events of a body include those of
  its child colliders, with the collider in the record.
- **Triggers** are Jolt sensors: they report and do not push. A trigger
  sees static bodies only when asked (`detectStatic`), as Jolt sensors
  skip them by default.
- **Order**: after each physics step, exits first, then enters, then stays,
  each sorted by node IDs, so a game behaves the same on every run.
- A body destroyed during a contact gets its exit events in the same
  frame; a callback never sees a destroyed node.

## Queries [planned, step 6]

- `physics().raycast(origin, direction, maxDistance, mask)` gives the
  nearest `RaycastHit` (node, collider, point, normal, distance) or none;
  `raycastAll` gives all hits by distance; `sphereCast`, `boxCast`,
  `capsuleCast` sweep a shape; `overlapSphere`, `overlapBox`,
  `overlapCapsule` give the colliders inside. A layer mask and
  `includeTriggers` filter them. They work in the edited scene too
  (placing objects by a tool).
- Queries read the state of the last step and may be called from any
  phase of the frame.

## Character controller [planned, step 6]

- **`CharacterController`** (on a `Node3D`, without a `RigidBody`): the Jolt
  virtual character, the established way to move a player through a world
  without the jitter of a dynamic body. Properties: the capsule (`radius`,
  `height`, `center`), `slopeLimit`, `stepHeight`, `skinWidth`, `mass` and
  `pushForce` for pushing dynamic bodies; it collides by the layer of its
  node, as bodies do.
- `move(velocity, delta)` moves it with sliding along walls, climbing
  steps and slopes up to the limit, sticking to the ground going down;
  `isGrounded()`, `groundNormal()`, `groundVelocity()` (a moving platform
  carries it); events `onControllerHit` with the node it touched.
- **Stairs and slopes as a person walks them**, each a property with a
  sensible default, built on the Jolt character helpers:
  - `walkStairs` (on): a step up to `stepHeight` is climbed without a jump
    and without slowing down (Jolt `WalkStairs`);
  - `stickToGround` (on) with `stickDistance`: going down stairs or a slope
    keeps the feet on the ground instead of flying off each step (Jolt
    `StickToFloor`); it lets go when the ground drops further, so a ledge
    is still a fall;
  - `slopeSpeed`: `KeepHorizontal` (default: the speed over the ground
    plan stays what the game asked, up or down, as a person walks) or
    `AlongSurface` (the speed is measured along the slope);
  - `slopeLimit`: steeper ground is a wall to walk into and slide down.
- `resize(height)` changes the capsule for crouching and refuses when
  there is no room above (a check of the new shape before it is taken).

**Where the engine ends and the game begins.** The controller moves a
capsule through the world correctly: it collides, slides, climbs steps,
keeps to the ground, stops at steep slopes, rides platforms and pushes
bodies. The game decides how the player wants to move: input into a
velocity, acceleration and turning, running and crouching, jumping and
gravity (the controller does not fall by itself, so a game may float,
double-jump or walk on walls), and animation. A ready player controller
built on it, with input actions and a camera, is the game part of
[navigation](navigation.md) (after the MVP).

## Joints [planned, step 6]

A joint links two bodies (or a body and the world) and limits how they move
relative to each other: chains, ropes, doors on hinges, drawers, wheels on
axles, ragdolls, things hanging from a ceiling.

- **A joint is a component** on a node with a `RigidBody`, one type per
  kind of joint. It links its body to `connectedBody` (a node reference to
  another body; empty links it to the world at its anchor).
  `allowMultiple`: a link of a chain may hold a joint to the previous link
  and another one to the next.
- **Kinds** (each the Jolt constraint of the same meaning, created and
  destroyed with the component):
  - `FixedJoint`: no relative motion (gluing, breakable walls);
  - `PointJoint`: one shared point, free rotation (chains, ropes);
  - `HingeJoint`: rotation about one axis (doors, wheels, levers);
  - `SliderJoint`: motion along one axis (drawers, pistons, lifts);
  - `DistanceJoint`: distance kept between a min and a max, with a spring
    (ropes that go slack, bungees);
  - `ConeJoint` and `SwingTwistJoint`: swing within a cone and twist within
    a range (ragdoll shoulders and hips, chain links that should not fold);
  - `SixDofJoint`: each of the six axes free, limited or locked, with
    springs (anything the others do not cover);
  - later: `PulleyJoint`, `GearJoint`, `RackAndPinionJoint`, `PathJoint`.
- **Common fields** (`Joint`, the base of all of them): `connectedBody`,
  `anchor` (in local space of the body; the anchor on the other body is
  computed from the scene when Play starts, so what is seen in the editor
  is what is linked), `axis` where the kind has one, `collideConnected`
  (off by default: linked bodies do not collide with each other),
  `breakForce` and `breakTorque` (0 means never), `enabled`.
- **Limits, springs and motors** are fields of the kinds that have them:
  `limits` (min, max in degrees or metres), `spring` (frequency and
  damping, so stiffness does not depend on mass), `motor` (`Off`,
  `Velocity`, `Position`, target and maximum force or torque). Scripts
  change the motor target to open a door or drive a wheel.
- **Breaking**: after every physics step the force of each joint with a
  limit is read from Jolt (the lambda of its last step divided by the
  step); above the limit the joint is disabled and `onBreak` is called on
  the scripts of its node. A broken joint stays as a disabled component,
  so a scene is not changed by a break.
- **Chains and ropes**: a chain is links (bodies) with `PointJoint` or
  `SwingTwistJoint` between them. The editor makes one with "Create >
  Chain": two end points in the viewport, the link (a model or a built-in
  capsule), the spacing; it creates the nodes, bodies and joints as one
  undo step, the ends attached to the nodes under the points or to the
  world. A soft rope and cloth (Jolt soft bodies) come later as their own
  component.
- **Stability**: a chain of many light links hanging a heavy body needs
  more solver iterations; `positionIterations` and `velocityIterations` are
  in the Physics section of the project settings, and each joint may raise
  them for itself (`solverIterationsOverride`).
- **Editor**: the anchor, the axis and the limits of the selected joint are
  drawn as gizmos (an arc for an angle limit, a segment for a slider), the
  anchor is moved with the manipulator; a line joins linked bodies.
- **Tests**: a body on a `PointJoint` to the world swings and stays at its
  length; a `HingeJoint` with limits stops at them; a motor in `Velocity`
  reaches its speed; `breakForce` breaks under a heavy body and calls
  `onBreak` once; a chain of 20 links holding a body 10 times heavier than a
  link does not stretch more than 1 percent with the default iterations;
  "Create > Chain" is one undo step.

## From code [planned, step 6]

Everything set in the inspector is a property with a setter (see
[scripting](scripting.md#scene-access-from-code-done)); on top of it:

- `RigidBody`: `velocity()` and `setVelocity`, `angularVelocity()` and
  `setAngularVelocity`, `addForce`, `addForceAtPoint`, `addImpulse`,
  `addTorque`, `addAngularImpulse` (with a `ForceMode`), `moveTo` and
  `rotateTo` for kinematic bodies (reaching the target at the next step),
  `sleep()`, `wakeUp()`, `isSleeping()`; `teleport` moves any body at once.
- `PhysicsWorld` through `physics()` of a script: the queries above,
  `gravity()` and `setGravity`, `ignoreCollision(a, b)` for two colliders.
- Events of bodies and triggers from code (`connect`) and wired in the
  inspector.

## Editor [planned, step 6]

- Colliders are drawn as wire shapes on the selected node (all of them
  with "Show colliders" on the viewport toolbar); their size and centre are
  changed with handles on the faces. A new collider fits the bounds of the
  mesh of its node (or its children); "Fit to mesh" does it again.
- The Physics section of the project settings: gravity, iterations, the
  default material and the collision matrix as a table of the named
  layers.
- Fields that take a body, a collider or a physics material accept drops
  from the tree and from Files.
- Play steps the physics; the edited scene does not move.

## Phases (a commit per phase)

### PH.1 Jolt and the world

- Jolt in the build (private, SSE4.2), `PhysicsRuntime` with the counted
  global setup of Jolt, trace and asserts into the log and the version
  check; `PhysicsWorld` with its implementation behind a pointer, created
  with the first body; the frame stage `systems.afterFixedUpdate`; object
  and broad phase layers from node layers and motion; the Physics section
  of the project settings (gravity, iterations, the collision matrix) with
  the matrix as a table of named layers in the settings window.
- Tests: the layer mapping and the filters (static never with static, a
  matrix entry turned off in both directions); the matrix stays symmetric
  whatever is written; a scene without bodies creates no Jolt world; a
  script force in `onFixedUpdate` comes before the step of the same fixed
  step (the order of the stages); the settings survive save and load.

### PH.2 Bodies and colliders

- `RigidBody`, the six colliders, compound shapes from descendants,
  implicit static bodies, scale, cooked mesh shapes, continuous collision,
  locks; the transform sync both ways with the remembered pose, the world
  pose computed by physics, interpolation; gravity of the settings and its
  change during Play.
- Tests: a box falls 1 s under gravity to 4.9 m within tolerance; a
  sphere rests on a static plane; two colliders on children form one body
  with the right mass; a kinematic platform carries a box; a fast small
  body with continuous collision does not pass a thin wall; non-uniform
  scale of a box collider; a mesh shape is cooked once for 100 colliders;
  a matrix entry turned off lets two bodies pass; a node moved by code in
  `onFixedUpdate` teleports its body in the same step; an interpolated
  body is never teleported by its own smoothed pose; the edited scene is
  not stepped; changing the fixed step changes the number of steps per
  second.

### PH.3 Physics materials

- `PhysicsMaterial` resource and its loader, combine modes in the contact
  listener, the project default.
- Tests: a box slides further on ice than on rubber; a ball bounces to the
  height its restitution gives; the combine modes.

### PH.4 Contacts and triggers

- The listener buffer, events after the step in a stable order,
  `Collision` record, triggers, exits on destruction.
- Tests: enter, stay and exit of one landing, once each; a trigger reports
  without pushing; events of a child collider reach the body; destroying
  a body inside a callback; the same order over ten runs.

### PH.5 Queries and the API from code

- Ray, shape casts and overlaps with masks and triggers; forces,
  velocities, kinematic moves, sleep; `physics()` of a script.
- Tests: a ray hits the nearest of three boxes and reports the normal; a
  mask skips a layer; `includeTriggers`; an impulse gives the expected
  velocity; a sleeping body wakes on a hit; queries in the edited scene.

### PH.6 Character controller

- `CharacterController` on the Jolt virtual character, moving, steps,
  slopes, pushing, platforms, its events.
- Tests: it walks up a step lower than `stepHeight` and stops at a higher
  one; running down stairs keeps it grounded every frame with
  `stickToGround` and leaves the ground without it; with `KeepHorizontal`
  its speed over the ground is the same on a slope up, a slope down and a
  flat floor; a ledge higher than `stickDistance` is a fall; `resize`
  refuses under a low ceiling; it slides along a wall at an angle; it does not climb a slope over
  the limit; it rides a moving platform; it pushes a light box and not a
  heavy one.

### PH.7 Joints

- The joints of "Joints" above with their tests.

### PH.8 Editor

- Collider gizmos and handles, "Show colliders", fitting to the mesh,
  drops onto physics fields; joint gizmos.
- Tests (logic without the interface): a new collider fits the mesh bounds
  of its node; a handle drag is one undo step.

### PH.9 Documents

This file, frame (physics in the frame), scene (bodies and colliders),
project settings (Physics), scripting (the API), the guide "Physics",
README.
