# Physics

[Русская версия](physics.ru.md)

Step 6 of the MVP: rigid bodies, colliders, queries and joints on
[Jolt Physics](https://github.com/jrouwe/JoltPhysics) (MIT). The plan of the
step (bodies, colliders, layers, queries, phases) is written here when the
step starts; this file already holds what was decided earlier.

Status marks: see [README](README.md).

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
