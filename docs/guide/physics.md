# Physics

[Русская версия](physics.ru.md)

Objects that fall, collide, bounce, slide and hang on each other, a player
that walks up stairs, zones that notice who entered them. Physics runs only
in Play and in a built game; the edited scene stands still. How it works
inside is in the [physics architecture](../architecture/physics.md).

Units are metres, kilograms and seconds; gravity pulls down along Y at
9.81 m/s2.

## 1. A falling box

1. Add a `Node3D` with a cube mesh (drag the built-in cube into the scene).
2. Add a `BoxCollider` to it: it takes the size of the cube at once.
3. Add a `RigidBody`: `motion` is `Dynamic`, `mass` 1 kg.
4. Add a floor: another cube scaled to (10, 0.2, 10) with only a
   `BoxCollider`. A collider without a body above it is a static body by
   itself, so a wall or a floor needs nothing more.
5. Press Play: the box falls and lands on the floor.

A body is made of the colliders of its node and of its children, down to
the next child with its own `RigidBody`: a table is one body of five box
colliders on its children. `motion` says how the body moves:

- `Dynamic` - pushed by gravity, forces and hits;
- `Kinematic` - moved only by code or animation, pushes dynamic bodies
  and carries what stands on it (lifts, moving platforms);
- `Static` - never moves.

## 2. Shapes of colliders

- `BoxCollider`, `SphereCollider`, `CapsuleCollider`, `CylinderCollider` -
  simple shapes, the fastest; use them whenever they are close enough.
- `ConvexCollider` - the convex hull of a mesh (a rock, a barrel): holes
  and dents are filled.
- `MeshCollider` - the exact triangles of a mesh: for level geometry, on
  static and kinematic bodies only.

The scene view draws the colliders of the selected node, and the
"Show colliders" button on its bar shows all of them. Drag the round
handles on the sides of a shape to change its size (the opposite side
stays), with Alt both sides move, with snapping on the size goes by the grid
step. "Fit to mesh" in the inspector of a collider fits it to the meshes of
its node again.

## 3. Friction and bounce

Files > Create resource > Physics material. A physics material has
`friction` (0 - ice, 0.6 - default, 1 and more - rubber) and `restitution`
(0 - no bounce, 1 - bounces back as high). Drop it onto the `material`
field of a collider. A collider without a material takes the default one
of the project (Project > Project Settings > Physics), where gravity, the
solver steps and which layers collide with which are set too.

## 4. Collisions and triggers in code

A `RigidBody` tells when it starts touching something, keeps touching and
stops: `onCollisionEnter`, `onCollisionStay`, `onCollisionExit`. The
`Collision` record has the other body's node, the colliders, the contact
points, the normal and `impulse`, the strength of the hit:

```cpp
class Crate : public yqlora::Script {
public:
    YQLORA_SCRIPT(Crate)

    float breakImpulse = 20.0f;  // how hard a hit breaks the crate

    YQLORA_FIELDS(field("breakImpulse", &Crate::breakImpulse).range(0.0f, 1000.0f))

    void onStart() override
    {
        auto* body = node().getComponent<yqlora::RigidBody>();
        body->onCollisionEnter
            .connect(*this, [this](const yqlora::Collision& hit) {
                if (hit.impulse > breakImpulse) {
                    scene().destroy(node());
                }
            })
            .release();
    }
};
```

The same events can be wired in the inspector without code, see
[events](../architecture/events.md).

A trigger is a zone that pushes nothing and reports who entered: turn on
`isTrigger` of a collider and listen to `onTriggerEnter` and
`onTriggerExit` of that collider (a checkpoint, a door that opens, water).
By default a trigger notices only moving bodies; `detectStatic` makes it
notice the rest.

## 5. Pushing bodies and asking the world

```cpp
auto* body = node().getComponent<RigidBody>();
body->addForce({0.0f, 0.0f, -10.0f});                           // newtons, this step
body->addForce({0.0f, 5.0f, 0.0f}, ForceMode::VelocityChange);  // a jump, mass does not matter
body->teleport({0.0f, 2.0f, 0.0f}, Quat(1.0f, 0.0f, 0.0f, 0.0f)); // move at once

// What is 10 m in front of the camera? Layers are a mask, triggers are skipped.
if (const auto hit = physics().raycast(origin, forward, 10.0f)) {
    debugDraw().point(hit->point, 0.2f);
}
```

`ForceMode` says how a vector is read: `Force` (newtons for the next step),
`Acceleration` (the same without mass), `Impulse` and `VelocityChange` (at
once). Queries also sweep shapes (`sphereCast`, `boxCast`, `capsuleCast`)
and find what is inside a shape (`overlapSphere`, `overlapBox`,
`overlapCapsule`). Gizmos > Physics shows the rays, contacts, velocities
and sleeping bodies in the scene and game views.

## 6. A player: the character controller

A body pushed by forces makes a jittery player. `CharacterController` (on a
node without a `RigidBody`) is a capsule that moves exactly as asked: it
slides along walls, walks up steps lower than `stepHeight`, keeps its feet
on the ground going down, stops at slopes steeper than `slopeLimit`, rides
platforms and pushes light bodies.

```cpp
void onUpdate(float delta) override
{
    auto* controller = node().getComponent<CharacterController>();
    Vec3 velocity = moveInput() * speed;   // the game decides how it wants to move
    if (!controller->isGrounded()) {
        m_fall -= 9.81f * delta;           // the controller does not fall by itself
    } else {
        m_fall = 0.0f;
    }
    velocity.y = m_fall;
    controller->move(velocity, delta);
}
```

Jumping, gravity, running and crouching (`resize`) are the game's, so a
game may float, double-jump or walk on walls.

## 7. Joints and chains

A joint links its body to another body (`connectedBody`) or to the world:
`HingeJoint` for a door or a wheel, `SliderJoint` for a drawer or a lift,
`PointJoint` and `DistanceJoint` for things hanging, `FixedJoint` to glue,
`ConeJoint` and `SwingTwistJoint` for shoulders and chain links,
`SixDofJoint` for the rest. A hinge and a slider have limits, a soft
spring at the limits and a motor: a script sets `motorTarget` to open a
door or turn a wheel. `breakForce` makes a joint break under a load and
call `onBreak`.

The scene view draws the joints of the selected node: the anchor, the
axis and the limits. Drag the anchor point to move it.

Right-click the scene tree > Create > Chain... makes a chain of capsule
links between two points; "Pick" puts a point with a click in the scene
view. Create > Body chain... makes a `PhysicsChain`: a long body that
bends by muscles (`setBend`) and crawls by grip, for a snake, a worm or a
tail.

## 8. Things to know

- Bodies step at the fixed step of the project (Project Settings > Time);
  add forces in `onFixedUpdate`. `interpolation` of a body smooths its
  motion between steps for the eye.
- A fast small body (a bullet) may pass through a thin wall: turn on
  `continuousCollision`.
- Long chains with a heavy load stretch: raise `solverStepsOverride` of
  their joints.
- Layers of nodes say what collides with what (Project Settings >
  Physics); two colliders can also be told to ignore each other from code
  (`physics().ignoreCollision`).
