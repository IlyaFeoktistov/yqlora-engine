# Physics

[Русская версия](physics.ru.md)

Step 6 of the MVP: rigid bodies, colliders, contacts and triggers, queries,
a character controller, joints and body chains on
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
- **Transforms**: physics reads the world pose of a node through
  `Node3D::worldMatrix()`, which is recomputed on read when a transform up
  the tree changed, so it is never old during fixed steps. Every body keeps
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
  on worker threads, so the listener only records touches into a buffer,
  and `PhysicsContacts` emits the events after the step, in a stable
  order.
- **Keeping bodies in step with the tree**: what a body is made of
  changes when a collider or a body enters or leaves the scene, is enabled
  or disabled, a property of it is set, or a branch is moved to another
  parent. Each of them marks the owner of the body as changed in the
  world: the components from `onEnterScene`, `onExitScene`, `onEnable`,
  `onDisable` and their setters, a move through a new scene system hook
  `onNodeMoved` that `Scene::setParent` calls. Before a step and before a
  query, the world rebuilds only the changed bodies: it finds which body
  each collider of the branch belongs to and builds the shape again; a
  body keeps its velocity when only its shape changes (`SetShape`) and is
  made again only when its motion changes. A child collider moved
  relative to its body (an arm of a robot) is found by the revision of the
  transforms between it and the body node (`Transform3D::revision()`), so
  nothing walks the tree every step.
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
- **Physics material** (`PhysicsMaterialData`), a resource (`.ymat` is
  taken by render materials, so `.yphysmat`, format "PhysicsMaterial"
  version 1): `friction` (0.6 by default), `restitution` (0, at most 1)
  and how two values combine (`Average`, `Minimum`, `Multiply`,
  `Maximum`). The data is a reflection record, so the file and the
  inspector take the fields from one description. `Collider::material` is
  the collider's own material; empty means the default material of the
  Physics section (`defaultMaterial`, the built-in "Default" at first).
  When the two modes differ, the higher one in maximum, multiply, minimum,
  average is used, so the result does not depend on which body is first.
- **Materials in Jolt**: every collider shape carries its material (a
  `JPH::PhysicsMaterial` subclass; mesh triangles share one, and mesh
  shapes are cooked per mesh and material), and the world's combine
  functions (`SetCombineFriction`, `SetCombineRestitution`) read the
  materials of both sub-shapes of a contact. There is one material object
  per resource for the life of the runtime: editing or reloading the
  resource changes its values before the step, bodies are not rebuilt,
  and a world that sees the change wakes its bodies so sleeping ones feel
  the new friction. Changing the default material in the settings
  rebuilds bodies: the material is in their shapes.

## Contacts and triggers [planned, step 6]

- **Collision events** (see [events](events.md)) on `RigidBody`:
  `onCollisionEnter`, `onCollisionStay`, `onCollisionExit` per collider
  pair, with a `Collision` record: the other body's node, its own collider
  and the other collider (`ComponentRef<Collider>`; a body is made of its
  descendants' colliders too, so a child collider's event reaches the
  body with that collider in the record), the contact points in the world,
  the normal (where the other body pushes this one), this body's velocity
  relative to the other at the contact point, and the impact impulse. Both
  sides with a `RigidBody` get it; a wall made only of colliders gets no
  events. The impulse is (1 + e) times the approach speed along the normal
  times the reduced mass of the pair, without rotation (Jolt does not hand
  out the solver impulse): for the strength of a hit in sound and damage;
  it is 0 for stay and exit, and an exit has no points.
- **Triggers**: `Collider::isTrigger` turns a collider into a zone that
  pushes nothing. A Jolt sensor is a whole body, not a part of a shape, so
  each trigger has its own kinematic sensor body, placed where the
  collider is every step and left out of the body's shape and mass. The
  sensor never sleeps: otherwise a body falling asleep inside the zone
  would leave it. `onTriggerEnter` and `onTriggerExit` on the trigger
  collider with the other body's node: enter when the body's first
  collider touches the zone, exit when its last one leaves; a body on the
  trigger's own node and other triggers do not count. A trigger sees
  static and kinematic bodies only when asked (`detectStatic`): a zone on
  a large floor would otherwise count every contact with it.
- **How enters and exits are found**: the Jolt listener records the step's
  touches (new and continuing) into a buffer, and `PhysicsContacts`
  compares them with the previous ones after the step: new is an enter,
  known is a stay, gone is an exit. A pair whose bodies both do not move
  (asleep or static) continues without events: Jolt is silent about
  sleeping ones. A part of a compound shape finds its collider by the
  number in the part's data, because Jolt reorders the parts.
- **Order**: after each physics step, exits first, then enters, then stays,
  each sorted by node and collider IDs, so a scene loaded from a file
  behaves the same on every run.
- **Leaving the scene**: a body or collider that leaves during a touch
  (destroyed, disabled) exits its touches after the next step; the side
  still in the scene gets the event, and the leaving side's node in the
  record is no longer found by its reference. A callback may destroy
  nodes: the step's events are gathered before the first call.

## Queries [planned, step 6]

- `physics().raycast(origin, direction, maxDistance, filter)` gives the
  nearest `RaycastHit` (the body's node, the collider, point, normal,
  distance) or none; `raycastAll` gives all hits, nearest first;
  `sphereCast`, `boxCast`, `capsuleCast` sweep a shape (a capsule's height
  includes its hemispheres, as in `CapsuleCollider`); `overlapSphere`,
  `overlapBox`, `overlapCapsule` give the colliders inside, by node and
  collider IDs. `QueryFilter` holds the layer mask and `includeTriggers`
  (triggers are skipped by default). Query shapes are built by the same
  code as collider shapes.
- Queries see bodies as the last step left them and may be called from
  any phase of the frame; changed bodies are rebuilt before a query. In a
  scene that does not step (the editor) bodies stand where their nodes
  are, so a placing tool hits what it sees.
- The queries live in `PhysicsQueries`, which only reads the Jolt world;
  `PhysicsWorld` prepares the bodies and hands the call over.

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
- **How it is built**: the capsule always stands upright and does not turn
  with the node; by default it is 1.8 m tall with a 0.4 m radius and its
  `center` (in world axes) puts the node at the feet. `move` works at
  once: the character first stands where the node is (code or the editor
  may have moved it), then moves, and the node is put where it got to.
  On walkable ground the downward part of the velocity is dropped (the
  feet follow the ground by `stickToGround`), the speed over the ground is
  laid onto the slope by `slopeSpeed`, and the velocity of the ground
  under the feet is added, so a platform carries it. `resize` keeps the
  feet in place. Changing the capsule or `skinWidth` rebuilds the
  character before its next move; the other properties are taken by every
  move.
- **What others see**: the character has an inner kinematic body of its
  capsule on the layer of its node, so bodies collide with it, queries hit
  it and triggers report it, all as its node (the collider in
  `RaycastHit` and `Collision` is empty). A node with both a controller
  and a `RigidBody` gets a warning in the log.
- The characters of a world are `PhysicsCharacters`, next to the bodies
  in the Jolt world of the scene.

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
  `anchor` (in the axes of its node), `collideConnected` (off by default:
  linked bodies do not collide with each other), `breakForce` and
  `breakTorque` (0 means never), `solverStepsOverride`, `enabled`. A joint
  says what it is through `definition()` (`JointDefinition`, a variant
  without physics library types, as `ColliderShape` is for colliders).
- **Limits, springs and motors** are fields of the kinds that have them.
  `HingeJoint` and `SliderJoint` share `AxisJoint`: `axis` (in the axes of
  the node), `limits` (`JointLimits`: on, min, max in degrees or metres;
  the start position is always inside), `limitSpring` (`JointSpring`:
  frequency and damping, so stiffness does not depend on mass), `motor`
  (`Off`, `Velocity`, `Position`), `motorTarget` and `maxMotorForce`
  (newtons or newton metres). `DistanceJoint`: `connectedAnchor`,
  `minDistance` and `maxDistance` (both below zero: the distance at
  creation), `spring`. Scripts change the motor target to open a door or
  drive a wheel; limits, springs and motors are applied to the live joint
  before the next step, without rebuilding it.
- **How it is built**: joints exist only in a running scene, created after
  the bodies are rebuilt, before a step (`PhysicsJoints`). The point and
  axes on the other body are taken from the scene at the first creation
  and remembered in the axes of its node, so what is seen in the editor is
  what is linked, and a rebuilt body does not shift the joint: a body that
  leaves the Jolt world first removes its joints, and they are built again
  after it. The other body is the first one of the Jolt constraint and the
  own body the second, so `HingeJoint::angle()` and
  `SliderJoint::position()` measure the own body along its axis from the
  start. Linked bodies that do not collide are a pair of bodies in
  `CollisionIgnores`.
- **Breaking**: after every physics step the force and torque of each
  joint are read from Jolt (the impulses of its last step divided by the
  step; `force()` and `torque()` from code); above a limit the joint is
  disabled and its `onBreak` is called, in the order of component IDs. A
  broken joint stays as a disabled component, so a scene is not changed by
  a break.
- **Chains and ropes**: a chain is links (bodies) with `PointJoint` or
  `SwingTwistJoint` between them. The editor makes one with "Create >
  Chain": two end points in the viewport, the link (a model or a built-in
  capsule), the spacing; it creates the nodes, bodies and joints as one
  undo step, the ends attached to the nodes under the points or to the
  world. A soft rope and cloth (Jolt soft bodies) come later as their own
  component.
- **Stability**: a chain of many light links hanging a heavy body needs
  more solver steps; `velocitySteps` and `positionSteps` are in the
  Physics section of the project settings (10 and 2 by default), and each
  joint may raise both for itself (`solverStepsOverride`). With the
  defaults a chain of 20 links holding a body 10 times heavier stretches
  by about 2 percent, with 20 steps by about 0.5 percent.
- **Editor**: the anchor, the axis and the limits of the selected joint are
  drawn as gizmos (an arc for an angle limit, a segment for a slider), the
  anchor is moved with the manipulator; a line joins linked bodies.
- **Tests**: a body on a `PointJoint` to the world swings and stays at its
  length; a `HingeJoint` with limits stops at them; a motor in `Velocity`
  reaches its speed; `breakForce` breaks under a heavy body and calls
  `onBreak` once; a chain of 20 links holding a body 10 times heavier than a
  link stretches less than 1 percent with `solverStepsOverride` 20, and
  less than without it; "Create > Chain" is one undo step.

## Body chains [planned, step 6]

A long body that bends along its length and moves by bending: a snake, a
worm, a tail, a tentacle. It is the established setup for such creatures
in a rigid body engine: a row of capsules linked by swing-twist joints with
motors, and grip against the ground that is stronger sideways than along
the body.

- **`PhysicsChain`** (on a `Node3D`, instead of a `RigidBody`): `segments`
  capsules of `radius` laid along the local Z axis of the node over
  `length`, the mass (`mass` in all) spread over them. Each link is a
  dynamic body owned by the component; neighbours are linked by a Jolt
  `SwingTwistConstraint` with `bendLimit` (the swing cone, degrees) and
  `twistLimit`; the links of one chain do not collide with each other (a
  Jolt group filter), the chain collides with the world by the layer of
  its node.
- **What is seen**: if the node has `segments` child nodes (made by
  "Create > Body chain" with a mesh on each, or by hand), each follows its
  link after every step, with interpolation like a body. With skinning
  (after the MVP, see [animation](animation.md)) the links drive the bones
  of one bending mesh instead.
- **Muscles**: every joint has a motor towards a target bend (two angles,
  sideways and up-down) with `stiffness` (spring frequency) and
  `maxTorque`. `setBend(index, side, up)`, `setBends(...)` for the whole
  chain, `bend(index)`, `segmentCount()`, `linkPosition(index)` and
  `linkVelocity(index)` from code; `relax()` turns the motors off (a dead
  snake falls limp).
- **Grip**: `sideGrip` and `forwardGrip` (0 to 1) damp the velocity of a
  link touching the ground across and along its axis. A snake crawls
  forward only because sideways slipping is harder than forward slipping,
  so this is a property of the engine, not of each game.
- **Body chains are joints and bodies of this file**: queries, contacts
  (with the link index in `Collision`), layers and materials work for
  links as for any body.

**Where the engine ends and the game begins.** The engine gives the chain,
its limits, the muscles, the grip and the API to set bends. How the
creature moves is the game: a travelling wave of bends along the body for a
snake (side to side) or a worm (up and down), steering, speed. A ready
"crawl forward" behaviour with its parameters is a plugin or an example
script, not the engine.

## From code [planned, step 6]

Everything set in the inspector is a property with a setter (see
[scripting](scripting.md#scene-access-from-code-done)); on top of it:

- `RigidBody`: `velocity()` and `setVelocity`, `angularVelocity()` and
  `setAngularVelocity` (set on a dynamic body; a kinematic one follows its
  node), `addForce`, `addForceAtPoint`, `addTorque` with a `ForceMode`:
  `Force` (newtons for the next step), `Acceleration` (the same without
  mass), `Impulse` (at once), `VelocityChange` (at once, without mass), so
  an impulse is one mode and not a second set of functions; `moveTo` and
  `rotateTo` for kinematic bodies (the node goes there at once, the body
  reaches it during the next step and carries what stands on it);
  `teleport` moves the node and any body at once, without a path;
  `sleep()`, `wakeUp()`, `isSleeping()`. Calls before the first step
  rebuild the body first, so a body made in `onStart` can be pushed at
  once. The movement methods are in the registry too (for Luau and
  visual scripts).
- `PhysicsWorld` through `physics()` of a script: the queries above,
  `gravity()` and `setGravity`, `ignoreCollision(a, b, ignore)` and
  `collisionIgnored` for two colliders (`CollisionIgnores`; the listener
  rejects their contacts, both bodies are woken when it changes).
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

### PH.2a Bodies and simple colliders

- `RigidBody`; `BoxCollider`, `SphereCollider`, `CapsuleCollider`,
  `CylinderCollider`; compound shapes from descendants; implicit static
  bodies; keeping bodies in step with the tree (`onNodeMoved`,
  `Transform3D::revision()`); the transform sync both ways with the
  remembered pose; interpolation; gravity of the settings and its change
  during Play.
- Tests: a box falls 1 s under gravity to 4.9 m within tolerance; a
  sphere rests on a static plane; two colliders on children form one body
  with the right mass; a collider added, disabled or moved to another
  parent changes the body; a child collider moved relative to its body
  rebuilds the shape; a kinematic platform carries a box; a matrix entry
  turned off lets two bodies pass; a node moved by code in
  `onFixedUpdate` teleports its body in the same step; an interpolated
  body is never teleported by its own smoothed pose; the edited scene is
  not stepped; changing the fixed step changes the number of steps per
  second.

### PH.2b Scale, mesh shapes and motion options

- Scale of nodes on shapes; `ConvexCollider` and `MeshCollider` with
  shapes cooked once per mesh and revision; continuous collision; locks;
  `startAsleep`.
- Tests: non-uniform scale of a box collider; a sphere under non-uniform
  scale takes the largest axis and says so once; a mesh shape is cooked
  once for 100 colliders and again after the mesh changes; a fast small
  body with continuous collision does not pass a thin wall; a locked axis
  does not move; a body that starts asleep wakes on a hit.

### PH.3 Physics materials

- The physics material resource, its loader and the built-in default,
  materials on shapes and the world's combine functions, the project
  default; in the editor, creation in Files, an inspector from the record
  fields, and editing resource files through one `ResourceEditing` for
  render and physics materials.
- Tests: a box slides down an icy slope and holds on a rubber one; a ball
  bounces to the height its restitution gives; the combine modes in either
  order; a file with a wrong field is refused, out-of-range numbers are
  held in range; a collider without a material takes the project value,
  and changing it in Play acts at once; an edited material acts from the
  next step and wakes a sleeping body; editing and writing the file in the
  editor.

### PH.4 Contacts and triggers

- The listener buffer, events after the step in a stable order, the
  `Collision` record, triggers on their own sensors with `detectStatic`,
  exits on leaving the scene.
- Tests: enter, stay and exit of one landing, once each, and a pair that
  falls asleep neither stays nor exits; a trigger reports without pushing;
  a body asleep inside a zone does not leave it; a trigger sees static
  bodies only with `detectStatic`; a child collider's events reach the
  body; a body destroyed in its own callback leaves and the other side
  gets the exit; the same order over ten loads of a saved scene.

### PH.5a Queries and the API from code

- Ray, shape sweeps and overlaps with a mask and triggers; forces with a
  `ForceMode`, velocities, kinematic moves, teleport, sleep; pairs without
  collisions; a script's `physics()`.
- Tests: a ray hits the nearest of three boxes and gives the normal, all
  hits come by distance; a mask skips a layer; `includeTriggers`; shape
  sweeps and overlaps; an impulse, a velocity change, a one-step force
  and a torque give the expected velocities; a sleeping body wakes on a
  hit, sleep and wake from code; a teleport is seen by a query before a
  step; queries in the edited scene see nodes where they were put; a pair
  without collisions passes through each other.

### PH.5b Debug drawing

- `DebugDraw` (see [rendering](rendering.md#debug-drawing-done)):
  primitives with a color, a lifetime and a depth test, channels, a line
  pass in the renderer, a script's `debugDraw()`, the "Gizmos" menu in
  the scene and game views. Physics channels (`PhysicsDebug.hpp`):
  collider shapes colored by static, kinematic and dynamic, trigger zones,
  contact points and normals, query rays and hits (a miss in another
  color), body velocities, sleeping bodies. Shapes, contacts and velocities
  are drawn by `PhysicsWorld::drawDebug` from the Jolt bodies when the
  frame is extracted (simple shapes with their own lines, convex hulls and
  meshes with the edges of their triangles), so the editor shows them in a
  scene that does not run; queries draw into `DebugDraw` when called
  (`QueryDebug`).
- Tests: a primitive with a lifetime lives that many seconds of game time
  and goes away; a channel that is off puts nothing into the frame; a
  query with the "Queries" channel on puts its ray and hit point; a
  contact puts its point and normal; the frame data of a view without
  debugging has no lines.

### PH.6 Character controller

- `CharacterController` on the Jolt virtual character, moving, steps,
  slopes, pushing, platforms, its events.
- Tests: it walks up a step lower than `stepHeight` and stops at a higher
  one; running down stairs keeps it grounded every frame with
  `stickToGround` and leaves the ground without it; with `KeepHorizontal`
  its speed over the ground is the same on a slope up, a slope down and a
  flat floor; a ledge higher than `stickDistance` is a fall; `resize`
  refuses under a low ceiling; it slides along a wall at an angle; it
  does not climb a slope over the limit; it rides a moving platform; it
  pushes a light box and not a heavy one; `onControllerHit` names the
  node once per new touch; triggers and queries see it as its node.

### PH.7a Joints: the base and simple kinds

- `Joint` and `AxisJoint`, `FixedJoint`, `PointJoint`, `HingeJoint`,
  `SliderJoint`, `DistanceJoint`; limits, springs and motors applied live;
  the point and axes on the other body remembered at creation; linked
  bodies that do not collide; force, torque, breaking and `onBreak`;
  `solverStepsOverride`.
- Tests: a body on a `PointJoint` to the world swings and keeps its
  length; a `HingeJoint` with limits stops at them; a motor in `Velocity`
  reaches its speed and a new target takes effect; a motor in `Position`
  brings a slider to its target within its limits; `breakForce` breaks
  under a heavy body and calls `onBreak` once, a joint under its limit
  holds and reports the weight; the chain of 20 links with raised solver
  steps; a `DistanceJoint` keeps its range; connected bodies collide only
  with `collideConnected`; a scene that does not run builds no joints.

### PH.7b Joints: cones, six axes and chains

- `ConeJoint`, `SwingTwistJoint`, `SixDofJoint`; "Create > Chain" in the
  editor.
- Tests: a cone stops swinging at its angle and a twist at its range; a
  locked axis of a `SixDofJoint` does not move and a free one does;
  "Create > Chain" is one undo step and its ends are attached to the nodes
  under the points.

### PH.8 Body chains

- `PhysicsChain` with its links, joints, group filter, segment nodes
  following the links, muscles, grip; "Create > Body chain".
- Tests: a chain of 10 links dropped on the ground lies along it without
  links passing through each other; a bend beyond `bendLimit` is stopped
  at the limit; the motors reach a target bend within tolerance; a
  travelling wave of bends with `sideGrip` above `forwardGrip` moves the
  chain forward along its axis, and with equal grip it hardly moves;
  `relax()` lets the chain fall limp; segment nodes follow their links.

### PH.9 Editor

- Collider gizmos and handles, "Show colliders", fitting to the mesh,
  drops onto physics fields; joint gizmos.
- Tests (logic without the interface): a new collider fits the mesh bounds
  of its node; a handle drag is one undo step.

### PH.10 Documents

This file, frame (physics in the frame), scene (bodies and colliders),
project settings (Physics), scripting (the API), the guide "Physics",
README.
