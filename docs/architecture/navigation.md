# Navigation and player control

[Русская версия](navigation.ru.md)

The map characters find paths on, and a ready component for player
control: walking with keys and by a mouse click. Comes after the MVP: it
builds on physics (step 6) and input actions.

Status marks: see [README](README.md).

## Decisions [planned]

- **A ready library**: building the map and finding paths is
  [Recast & Detour](https://github.com/recastnavigation/recastnavigation)
  (zlib). Recast builds the map from geometry, Detour finds paths and
  nearest points on it, DetourCrowd steers groups of agents around each
  other.
- **Three parts, each on its own**: the navigation map (a resource, knows
  nothing of the scene), the agent (a component that walks the map) and
  player control (a component that turns input into a movement target).
  The agent also serves enemies driven by a script, player control is
  only a source of targets.
- **Scripts get all of it**: a path from point to point, the nearest point
  of the map, a ray along the map, the target of an agent.

## Navigation map [planned]

- **A navigation region** is a `NavigationRegion3D` node: what is nested in
  it is baked (meshes and colliders; the `navigation` flag of a
  `MeshRenderer` excludes a mesh). Parameters of the agent the map is built
  for: radius, height, max slope, step height; cell and tile size.
- **Baking** in the editor with a button in the region inspector, in the
  background, without stalling the window; the result is a `.ynavmesh`
  resource next to the scene, referenced by the region. A scene without a
  baked map works, path finding just finds nothing, and the log says so
  once.
- **Areas with a cost**: part of the geometry has its own area
  (`Walkable`, `Water`, `Road`...), an agent has a cost for each, so a path
  prefers the road and avoids water. The area list lives in the project
  settings.
- **Links** are a `NavigationLink3D` node between two points (a jump off a
  ledge, a ladder, a teleport): Recast does not find such places itself.
- **Moving obstacles** (doors, crates) cut holes in the map at run time
  through tiles (DetourTileCache), without rebaking it whole.
- **The editor** draws the map over the scene as a debug view of the
  viewport (walkable surface, areas by colour, links as arcs).

## Agent [planned]

- **The `NavigationAgent` component** on a `Node3D`: `destination`, speed,
  acceleration, turning to the direction of movement, avoidance radius,
  priority. The path is recomputed when the target or the map changes.
- **Avoidance** is DetourCrowd: agents of one map do not walk through each
  other and pass in narrow places.
- **Movement through physics**: if the node has a character controller
  (step 6), the agent gives it the desired velocity, and gravity, steps and
  collisions stay with physics; without physics the agent moves the node
  itself along the height of the map.
- **Events to scripts**: arrived, no path found, stuck.

## Player control [planned]

- **The `PlayerController` component** on the player node: ready movement
  without a script of your own. Each way is switched on separately, each
  with its own parameters:
  - **keys**: the `MoveX`, `MoveY` actions (WASD and the left stick by
    default, see [input](input.md)), direction relative to the camera or
    the world, speed, sprint (`Sprint`), jump (`Jump`), acceleration and
    deceleration;
  - **mouse click**: a ray from the main camera through the cursor, the hit
    point is brought to the nearest point of the navigation map, and the
    `NavigationAgent` of the same node walks there; holding the button
    updates the target; an optional target marker in the world.
- Pressing a movement key interrupts the click path, so both ways work
  together.
- **The camera** does not belong to the controller: a third person, top
  down or first person camera is its own component, the controller takes
  only the direction from it.
- **Tests**: on a baked plane with an obstacle the path goes around it; an
  agent reaches the target and fires the event; two agents facing each
  other in a corridor pass; the `MoveY` axis moves the player forward
  relative to the camera; a click on a point off the map leads to the
  nearest point of the map; a movement key interrupts the click path.
