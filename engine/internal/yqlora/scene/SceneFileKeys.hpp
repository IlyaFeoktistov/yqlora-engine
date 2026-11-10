#pragma once

namespace yqlora::scene_keys {

// Ключи файла сцены и префаба (.yscene, .yprefab) и веток в их формате.
inline constexpr const char* kRoot = "root";
inline constexpr const char* kType = "type";
inline constexpr const char* kId = "id";
inline constexpr const char* kProperties = "properties";
inline constexpr const char* kComponents = "components";
inline constexpr const char* kChildren = "children";
// Свойство имени узла: у корня экземпляра оно всегда своё.
inline constexpr const char* kNameProperty = "name";

// Запись экземпляра префаба на месте узла (см. docs/architecture/prefabs.md):
// префаб, ID экземпляра ("id") и переопределения по видам.
inline constexpr const char* kPrefab = "prefab";
inline constexpr const char* kOverrides = "overrides";
inline constexpr const char* kAdded = "added";
inline constexpr const char* kAddedComponents = "addedComponents";
inline constexpr const char* kRemoved = "removed";
inline constexpr const char* kRemovedComponents = "removedComponents";
// Поля переопределений: цель в пространстве префаба и значение.
inline constexpr const char* kNode = "node";
inline constexpr const char* kComponent = "component";
inline constexpr const char* kParent = "parent";
inline constexpr const char* kProperty = "property";
inline constexpr const char* kValue = "value";

} // namespace yqlora::scene_keys
