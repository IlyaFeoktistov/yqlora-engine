# Types and reflection

[Русская версия](reflection.ru.md)

The type registry describes every type of node, component and scene system:
its name, base, module, properties, methods and component rules. The editor
builds menus and the inspector from it, scene files create objects by type
name, script bindings and the visual script palette are generated from it.

Status marks: see [README](README.md).

## Types [done]

- A type has a name, a base of the same kind (`TypeKind::Node`, `Component`,
  `System`) and the module that registered it. The roots `Node`, `Component`
  and `SceneSystem` have no base; a base is registered before its derived
  types.
- A C++ class names itself and its base once, with `YQLORA_TYPE(Class, Base)`;
  `registerNode<T>()`, `registerComponent<T>()` and `registerSystem<T>()` take
  both from there.
- A module is the engine (`engine`), the game (`TypeRegistry::ModuleScope`,
  for a game library through `ModuleRegistrar`, see [scripts](scripting.md)),
  later Luau scripts and visual scripts. `removeModule` takes all types of a
  module out before its code is unloaded.
- A renamed type keeps its old name with `formerTypeName("Old")`
  (`formerly` for scripts): `find` falls back to former names, so files
  written under the old name load, and the new name is written.
- Component rules (`requiresNode`, `requiresComponent`, `allowMultiple`,
  execution order) are part of the type, see [scene](scene.md).

## Properties [done]

- A property (`Property`) is a name, a type of `PropertyValue` (bool, int,
  float, string, vectors, enum, node reference, resource reference), an
  inspector hint (`PropertyHint`) and functions that read and write the value.
  `ClassTypeBuilder::property` makes them from a getter and a setter or from a
  field.
- Registered properties are what scene files save, the inspector shows and
  scripts see.

## Property attributes [done]

How a property is shown and saved, the same for every language that declares
types (`PropertyAttributes`): `range(min, max)` limits numbers in the
inspector (also the numbers of vectors and lists), `displayName` and
`tooltip` (a translation key or the text itself), `group` puts it under a
heading, `notSaved` keeps it out of files, `notShown` out of the inspector,
`formerly("oldName")` reads files written under an old name,
`resource(kind)` tells which resources a reference (or a list of them) takes
(`T::kResourceKind`, the inspector offers only those). They chain after
`property(...)` in a type builder and after `field(...)` in records and
scripts. Saving and the inspector are two separate users of them.

## Enum value names [done]

```cpp
YQLORA_ENUM_NAMES(yqlora::CameraClear, "ColorAndDepth", "DepthOnly", "Nothing")
```

- The names of the values of an enum are one table next to the enum
  (`EnumNames<E>`, by value order 0, 1, ...). Scene and settings files,
  `.meta`, the inspector and the log all read it (`enumName`,
  `enumFromName`), so a value is written by name and the C++ order can
  change.
- A property of an enum type takes the names by itself; an enum without
  the table does not compile as a property. Types described at run time
  (Luau, graphs) give the names with `Property::setEnumNames`.

## Methods [done]

```cpp
registry.registerComponent<Transform3D>()
    .method("lookAt", &Transform3D::lookAt, {{"target"}, {"up", math::kWorldUp}});

const std::vector<PropertyValue> arguments{target};
MethodCallResult result = lookAt.call(transform, arguments); // called is false if the arguments do not fit
```

- A method (`Method`) is, like a property, a function over `PropertyValue`:
  name, parameters with types and optional trailing defaults, result type.
  `ClassTypeBuilder::method` takes the types from the C++ signature and only
  the parameter names from the caller, `Method::forComponent` describes a
  method of a runtime type. A returned node pointer becomes a `NodeRef`.
- Engine methods so far: `Node.findChild`, `Node3D.worldPosition` /
  `setWorldPosition` / `toWorld` / `toLocal` / `worldForward` / `worldRight` /
  `worldUp`, `Transform3D.lookAt`.
- Properties and methods are one list for Luau bindings, the visual script
  palette and the API documentation.

## Types declared at run time [done]

A Luau script or a visual script graph is a type without a C++ class of its
own. `registry.registerType(name, kind, base)` returns a `TypeBuilder` that
takes the factory, the lifecycle phases, properties made of functions
(`Property::forComponent`) and methods (`Method::forComponent`); an instance
returns its own type name from `typeName()`. Types of C++ classes go through
the same path (`ClassTypeBuilder` turns members into those values), and the
scene indexes components by type name, so every such type gets lists of its
own.
