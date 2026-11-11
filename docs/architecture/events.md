# Events

[Русская версия](events.ru.md)

An event is something a component tells others about: a timer ran out, a
button was pressed, a body entered a trigger. Code subscribes to it, and a
scene can wire it in the inspector without code: "when this button is
pressed, call `open` on that door". Timers, the game interface (step 8) and
physics (step 6) are built on it.

Status marks: see [README](README.md).

## Events from code [done]

```cpp
class Score : public Script {
public:
    YQLORA_SCRIPT(Score)
    Event<int> onScored;                     // a member of the component
    void add(int points) { onScored.emit(points); }
};
YQLORA_REGISTER_SCRIPT(Score).event("onScored", &Score::onScored, {"points"});

// another component:
Connection c = score->onScored.connect(*this, [this](int points) { show(points); });
```

- **`Event<Args...>`** (`scene/Event.hpp`) is a member of a component and
  is registered with the names of its arguments
  (`.event("onTimeout", &Timer::onTimeout)`, `EventInfo` in the registry,
  `TypeRegistry::events`). The argument types are property types, so Luau
  and visual scripts listen through `connectValues`, which gets the
  arguments as `PropertyValue`s.
- **The owner** of a connection is a component: when it leaves the scene
  its connections are dropped (`Scene::exitComponent` puts out the life
  mark of the component), so a callback of a destroyed component is never
  called. A `Connection` disconnects on `disconnect()` or when it is
  destroyed; `release()` lets it live as long as the owner or the event.
- **Emitting** (`emit(args...)`): code listeners first, in order of
  connection, then persistent ones in list order. A listener added during
  an emit is first called by the next emit, one removed during an emit is
  not called any more. An exception in a listener is caught and logged with
  the event and the listener, and the others still run, as calls of the
  lifecycle are guarded.
- **No global message bus**: an event belongs to a component, so who can
  emit and who listens is visible in code and in the inspector.

## Persistent listeners [done]

- `.event(...)` also adds a property with the name of the event
  (`PropertyHint::EventTargets`): a list of `EventTarget`s, each a node
  (`NodeRef`), the node itself or its component by `ComponentId`, and a
  method name. Being a property, the list is saved with the scene, edited
  in the inspector with undo and copied into Play; a target inside a
  prefab is mapped like any `NodeRef` (see [prefabs](prefabs.md)).
- When the event is emitted, each target's method is called through the
  registry (`Method::call`). A method with parameters gets the event
  arguments, one without parameters is called without them, so it listens
  to any event. A target that is gone or whose method does not fit is
  skipped and reported in the log once, not every emit.
- **Only in a running scene**: persistent listeners are called where
  `Scene::playing()` is set (the Play copy, a game; `PlayMode` and
  `GameRuntime` set it), never in the edited scene of the editor.

## Editor [done]

- An event property shows its persistent listeners as a list
  (`EventTargetsField`): for each, a node field (a node is dropped from the
  tree onto it), then the node or one of its components, then a method
  that listens to the event (`eventMethods`: no result, and no parameters
  or the event's parameter types in order), with the names of the
  parameters; a button removes the row. "Add" at the bottom takes a dropped
  node too. One undo step per change.
