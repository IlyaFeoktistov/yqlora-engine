# Events

[Русская версия](events.ru.md)

An event is something a component tells others about: a timer ran out, a
button was pressed, a body entered a trigger. Code subscribes to it, and a
scene can wire it in the inspector without code: "when this button is
pressed, call `open` on that door". Timers, the game interface (step 8) and
physics (step 6) are built on it.

Status marks: see [README](README.md).

## Decisions [planned]

The design is the established one for component events: listeners from code
and persistent listeners saved with the scene, called through reflection.

- **`Event<Args...>`** is a member of a component and is registered like a
  property (`.event("onTimeout", &Timer::onTimeout)`), with the names of its
  arguments; its argument types are property types, so Luau and visual
  scripts get events without extra work.
- **Listeners from code**: `connect(owner, callback)` returns a
  `Connection`. The owner is a component: when it leaves the scene its
  connections are dropped, so a callback of a destroyed component is never
  called. A `Connection` disconnects on `disconnect()` or when it is
  destroyed, unless `release()`d to live as long as the owner.
- **Persistent listeners** are the value of the event property: a list of
  targets (`NodeRef`, the component by its `ComponentId`, the method name),
  set in the inspector and saved with the scene. When the event is
  emitted, the method is called through the registry (`Method::call`) with
  the event arguments; a method whose parameters do not fit is reported
  once and skipped. A target inside a prefab is mapped like any `NodeRef`
  (see [prefabs](prefabs.md)).
- **Emitting** (`emit(args...)`): code listeners first, in order of
  connection, then persistent ones in list order. A listener added during
  an emit is first called by the next emit, one removed during an emit is
  not called any more. An exception in a listener is caught and logged with
  the event and the listener, and the others still run, as calls of the
  lifecycle are guarded.
- **Only in the scene where it was set**: a persistent listener is called
  in the Play copy and in a game, never in the edited scene of the editor.
- **No global message bus**: an event belongs to a component, so who can
  emit and who listens is visible in code and in the inspector.

## Editor

- An event property shows its persistent listeners as a list: a node field
  (dropping a node from the tree onto it), then a list of the components of
  that node, then their methods whose parameters fit the event, with the
  names of the parameters. One undo step per change.

## Phases (a commit per phase)

### EV.1 Events from code

- `Event<Args...>`, `Connection`, owner-bound connections, the order and
  re-entrancy rules, guarded calls; `.event(...)` in the registry.
- Tests: a callback of a destroyed owner is never called; connect and
  disconnect during an emit; an exception in one listener does not stop
  the next; a `Connection` destroyed disconnects.

### EV.2 Persistent listeners

- The event property value (`EventValue`: a list of targets), its
  serialization, calls through `Method::call`, a target that does not fit
  reported once; the inspector list.
- Tests: a listener set on one node calls a method of another after save
  and load; a removed target is skipped without an error loop; listeners do
  not run in the edited scene; one undo step per change.

### EV.3 Documents

This file, reflection (event properties), scripting, editor.
