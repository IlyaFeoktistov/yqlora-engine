# Engine context and the frame

[Русская версия](frame.ru.md)

What the process shares, what every scene owns, and the order of one frame
of the engine.

Status marks: see [README](README.md).

## Engine context

Three levels: what the process shares, what every scene owns, what a component
sees.

```
Owner (game / editor / test)
 `- EngineServices     shared by the process: input, project, type registry, scene requests
     `- Scene          owned by every scene
         |- GameTime        game time, time scale, fixed step
         |- systems         render, physics, audio... (pluggable)
         |- phase lists     components to call in fixedUpdate / update / lateUpdate
         `- node tree
             `- Component -> owner()->scene() -> services(), time(), system<T>()
```

### Engine services [done]

```cpp
class EngineServices {
public:
    const Input& input() const;          // raw state + actions from the project settings
    const Project* project() const;      // settings and resources; nullptr when there is none
    const TypeRegistry& registry() const;
    SceneRequests& scenes();             // "load a level": done between frames  [planned, step 9]
};
```

- A scene gets its services when it is created: `Scene(const EngineServices&)`.
  Tests and empty scenes get services with nothing pressed and an empty
  project, so no code checks for `nullptr`.
- No `Application::instance()`. Log and translation stay global by design.
- The registry in the services is what lets `Node::addComponent` check the
  component rules itself and what tells the scene the lifecycle phases of a
  type.
- In the editor the Play copy has its own services with its own `Input`: it
  sees the input of the application only while the Game window is focused,
  so keys typed in the inspector do not move the player.

### Game time [done]

```cpp
class GameTime {
public:
    double elapsed() const;          // since the game started, scaled
    float delta() const;             // this frame, scaled
    float unscaledDelta() const;     // real step: editor, HUD animations
    float fixedStep() const;         // from the Time section of the project settings
    float scale() const;
    void setScale(float scale);      // 0 stops the game, 0.5 is slow motion
    std::uint64_t frame() const;
};
```

Every scene has its own; the Play copy starts at zero and pause stops it. The
real frame length comes from `FrameClock`, game time is derived from it.

### Scene systems [done]

The main extension point: a module adds a system and its components; `Scene`,
the frame loop and the application do not change.

```cpp
class PhysicsWorld : public SceneSystem {
public:
    YQLORA_TYPE(PhysicsWorld, SceneSystem)
    explicit PhysicsWorld(Scene& scene);

    void fixedUpdate(float step) override;     // physics steps here
    // also update, lateUpdate (after scripts: render list, audio)
    // and updateTransforms (after the simulation, before extract)
};

// A module registers the type once; every new scene gets an instance:
registry.registerSystem<PhysicsWorld>();
scene.system<PhysicsWorld>()                    // a component registers its body in onEnterScene
```

- Systems are types of the registry (`TypeKind::System`), so they belong to a
  module and leave with it like components. A scene creates one instance of
  every system type when it is constructed. A game module registers only
  scripts (`ModuleRegistrar`), so its reload touches no systems **[done]**;
  systems of a reloaded plugin module are in [plugins](plugins.md).
- Systems are called in the order of registration. An explicit order comes
  with the first pair of systems that needs one.
- The edited scene in the editor has its systems too (bodies exist, colliders
  can be drawn), but only the Play copy is stepped.
- `TransformSystem3D` is the first system: it keeps the world matrices of 3D
  nodes fresh, see the frame pipeline.

### One frame [done]

The order lives in one place, `Scene::advance`:

```
scene.advance(realDelta):
    time.advance(realDelta)                       scale, fixed-step accumulator
    while a fixed step is accumulated:
        systems.fixedUpdate, then components.onFixedUpdate
    components.onUpdate
    systems.update
    components.onLateUpdate
    systems.lateUpdate
    apply deferred tree changes                   destroy, change of parent
```

The game (`Application`) calls `advance` every frame, the editor's `PlayMode`
calls it for the Play copy when not paused, tests call it directly.

## Frame pipeline

The whole engine frame, not only the scene part. Each stage has one owner;
the order lives in one frame loop class that knows only the order
**[done]**: `FrameLoop` calls the stages of `FrameStages`, the owner (the
game's `Application`, a test) says what each stage does.

```
1. Begin       FrameClock: frame length
2. Input       window events -> raw input -> actions from the project settings
3. Simulate    scene.advance: fixed steps (physics) -> update -> late update
               -> deferred tree changes
4. Transforms  world matrices of changed nodes, one pass with a dirty flag
5. Extract     the render system takes a snapshot of the scene: views (cameras),
               what to draw, lights. Nothing reads the scene after this.
6. Render      prepare: culling by visibility and layer mask per view, sorting
               (opaque front to back, transparent back to front)
               graph: render passes (shadows, opaque, sky, transparent,
               post-processing, HUD) through the RHI
7. Interface   ImGui of the editor and tools
8. Present
```

- **In the editor** the scene windows (viewport, game, camera preview) do
  their own extract and render during the interface stage: each has its own
  camera and texture. The Play copy is simulated and its transforms are
  updated in the simulate stage, by `PlayMode`, in the same order.

- **The snapshot from Extract** is all the renderer reads; it never touches
  the scene. Later the renderer can move to its own thread (the game computes
  frame N+1 while frame N is drawn) without changing game code. Extract is
  `extractFrame(scene, ...)`: the views of the render system's cameras and
  what to draw, with layers **[done]**.
- **World matrices** are cached with a dirty flag: a change of a transform
  marks the node and its branch, a read recomputes only what is stale
  **[done]**. `TransformSystem3D` queues the top changed node of every branch
  and refreshes them in one pass between simulation and extract
  (`scene.updateTransforms()`), so extract reads ready values **[done]**.
- **The render graph**: a pass declares what it reads and writes, the graph
  orders the passes. A new effect is one more pass, not a change of the
  renderer. The editor adds its own passes (grid, selection outline) to the
  graph through `ViewData::passes`; `FrameData::editor` is gone. Culling and
  sorting per view came with it **[done]**, see
  [rendering](rendering.md#prepare-and-the-render-graph).
- **The render thread** comes when it is needed; the snapshot from Extract
  already allows it **[planned, later]**.

## The application [done]

`Application` assembles the parts and runs the frame loop, nothing else.

- It creates the window, the renderer and, for tools, the interface layer,
  and runs `FrameLoop`.
- The scene of a game is held by `GameRuntime`, which later also switches
  scenes **[planned, step 9]**. The editor has no scene of the application:
  its scenes are the document and the Play copy.
- The frame stages differ by mode: a game extracts and renders its scene, a
  tool draws its interface. No `if (ui)` inside the stages and no hooks for
  the editor (`setSceneRunning`, `onPrepareFrame`).
- The interface language is a service of its own: it picks the language on
  the first start, switches it and remembers the choice.
- The folders of user data and of engine assets come from platform
  functions, not from methods of the application.
