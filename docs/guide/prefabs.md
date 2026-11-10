# Prefabs

[Русская версия](prefabs.ru.md)

A prefab is an object built once and placed many times: a door, an enemy, a
lamp with its light. Every copy in a scene (an instance) stays linked to the
prefab: an edit of the prefab reaches all copies, and what is changed in one
copy stays with that copy only. How it works inside is in the
[prefab architecture](../architecture/prefabs.md).

## 1. Make a prefab

- In Files: Create resource > Prefab, a name, Create. This makes a
  `.yprefab` file with an empty `Node3D`.
- Or from a ready branch of the scene: right-click a node in the tree >
  "Save branch as prefab...", or drag the node from the tree onto a folder
  in Files. The branch is written into the prefab, and its instance takes
  the branch's place in the scene; references of other nodes to the branch
  nodes now lead into the instance. Ctrl+Z brings the branch back, the
  prefab file stays.

A model (glTF) is a prefab too, a read-only one: its nodes come from the
file.

## 2. Put it into a scene

Drag a prefab or a model from Files into the viewport, onto a node of the
tree or onto the empty place under the tree, as a model in
[importing a model](import-model.md). Instance nodes have their own colour
in the tree, the root has the prefab icon. A scene cannot be placed this
way: a level does not go into a level.

The position, rotation, scale and name of the root belong to each instance.

## 3. Edit a prefab

A double click on a prefab in Files (or "Open prefab" in the inspector of an
instance) opens it in the scene window, like a scene. A prefab usually has
no sky and sun of its own, so the scene window lights it with preview
lighting, the sun and sky of a new scene; this light does not go into the
file, and the sun button on the scene window bar turns it off.

Save the prefab (Ctrl+S), and all its instances in scenes are updated.

## 4. Change one instance

Select a node of an instance and change it as usual: properties,
components, children. A changed property has a coloured label in the
inspector, an added component a coloured header. Right-click a property or
a component header:

- **Revert to prefab** - the value is as in the prefab again;
- **Apply to prefab** - write the value into the prefab: every instance
  gets it.

The instance root shows its prefab at the top of the inspector and the
buttons "Open prefab", "Revert all", "Apply all", "Unpack". "Unpack" turns
the instance into ordinary nodes with no link to the prefab. Each of these
actions is undone by one Ctrl+Z.

Prefab nodes inside an instance can be deleted and get new children, but
not moved to another parent.

## 5. Nested prefabs and variants

An instance of one prefab can go into another: a house with doors. An edit
of the door reaches every house. A prefab cannot contain itself: such a
drop is not taken.

A variant is a prefab based on another: right-click a prefab (or a model)
in Files > "Create variant...". The variant gets everything from its base
and keeps only its own differences: a blue door, a big door.

## 6. From code

Rare objects (a wave enemy, an item) are made with `instantiate`, frequent
ones (bullets, shells) come from a `PrefabPool`, so the prefab is not
expanded on every shot.

Add a `PrefabPool` to a node in the inspector, choose the bullet prefab and
the size in it, and give the gun a script:

```cpp
#pragma once

#include <yqlora/scene/Prefab.hpp>
#include <yqlora/scene/PrefabPool.hpp>
#include <yqlora/scripting/Script.hpp>

class Gun : public yqlora::Script {
public:
    YQLORA_SCRIPT(Gun)

    yqlora::Ref<yqlora::Prefab> crate;
    yqlora::ComponentRef<yqlora::PrefabPool> bullets;

    YQLORA_FIELDS(field("crate", &Gun::crate), field("bullets", &Gun::bullets))

    void dropCrate();
    void fire();
};
```

```cpp
#include "Gun.hpp"

#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scripting/ModuleScripts.hpp>

YQLORA_REGISTER_SCRIPT(Gun).requiresNode<yqlora::Node3D>();

void Gun::dropCrate()
{
    // A new instance as a child of the scene root; in a game from the next frame.
    if (auto* box = static_cast<yqlora::Node3D*>(scene().instantiate(crate, *scene().root()))) {
        box->setWorldPosition(static_cast<yqlora::Node3D&>(node()).worldPosition());
    }
}

void Gun::fire()
{
    yqlora::PrefabPool* pool = scene().find(bullets);
    auto* bullet = pool ? static_cast<yqlora::Node3D*>(pool->acquire(*scene().root())) : nullptr;
    if (!bullet) {
        return;
    }
    bullet->setWorldPosition(static_cast<yqlora::Node3D&>(node()).worldPosition());
    // In two seconds the bullet goes back into the pool with the prefab values.
    const yqlora::NodeId id = bullet->id();
    timers().after(2.0f, [this, id] {
        yqlora::PrefabPool* owner = scene().find(bullets);
        if (yqlora::Node* used = scene().find(id); owner && used) {
            owner->release(*used);
        }
    });
}
```

The `crate` and `bullets` fields are filled in the inspector: the prefab
from Files, the pool by dragging the node with the `PrefabPool` from the
tree. A returned bullet gets the prefab values; whatever the bullet script
keeps outside its fields it resets itself in `onEnable` or `onDisable`.

## If something is wrong

- **The prefab is dark in the scene window**: turn on preview lighting (the
  sun button on the scene window bar) or give the prefab its own light.
- **"Apply" is unavailable**: the prefab of the instance is a model, which
  is read-only. Make a variant of the model: it is a `.yprefab` and can be
  applied to.
- **"Unused overrides" in the inspector**: changes of the instance whose
  target is no longer in the prefab (the node was removed from the prefab).
  They are kept while the target may come back; "Remove unused" deletes
  them.
- **A prefab was deleted, its instances remain**: they stay as they were
  and are saved; the next time the scene is opened, placeholder nodes stand
  in their place and keep everything until the file comes back (the log
  tells which prefab is not found).
- **A prefab is not dropped**: it is a scene, or the open prefab is already
  contained in the dropped one (that would be a cycle).
