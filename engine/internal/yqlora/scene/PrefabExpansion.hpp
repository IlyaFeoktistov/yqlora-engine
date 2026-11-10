#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace yqlora {

class EngineServices;
class PrefabBases;
class ResourceCache;
class ResourceDatabase;
class TypeRegistry;

// Развёртывание экземпляров префабов и переопределения (см.
// docs/architecture/prefabs.md). Всё здесь - над ветками в формате файла сцены
// (SceneBranches): узлы из них строит и пишет SceneSerializer.
//
// Пространства ID. Дерево префаба - в своём пространстве: ID узлов и
// компонентов как в его файле. В экземпляре ID объекта из префаба выведен:
// deriveId(ID экземпляра, ID в префабе); узлы и компоненты, добавленные в
// экземпляр, и ссылки наружу остаются как есть. Переопределения записи - в
// пространстве префаба, поэтому одинаковы у всех экземпляров.

// Где брать данные префабов.
struct PrefabSources {
    ResourceCache& cache;
    // nullptr - проекта нет: у моделей нет материалов из файла.
    const ResourceDatabase* resources = nullptr;
    // Кэш развёрнутых деревьев; nullptr - каждое разворачивается заново.
    PrefabBases* bases = nullptr;

    YQLORA_API static PrefabSources of(const EngineServices& services);
};

// Узел ветки - запись экземпляра префаба, а не узел.
YQLORA_API bool isPrefabRecord(const nlohmann::json& node);

// Префаб и ID экземпляра из записи; 0, если их нет или они испорчены.
YQLORA_API ResourceId recordPrefab(const nlohmann::json& record);
YQLORA_API NodeId recordInstance(const nlohmann::json& record);

// Имя корня по записи, для узла на месте экземпляра, чей префаб не
// развернулся: имя корня - первое переопределение имени узла в записи
// (computeOverrides пишет его первым).
YQLORA_API std::optional<std::string> recordName(const nlohmann::json& record);

// Дерево префаба: корень файла .yprefab или ветка модели (modelBranch).
// nullptr с причиной в логе, если prefab - не префаб и не модель (сцена
// тоже) или не загрузился.
YQLORA_API std::shared_ptr<const nlohmann::json> prefabTree(
    ResourceId prefab,
    const PrefabSources& sources,
    const TypeRegistry& registry
);

// Ветка без записей экземпляров: каждая развёрнута (expandPrefab) из текущих
// данных своего префаба, вложенные - так же, в пространстве ветки. chain -
// префабы, которые сейчас разворачиваются, от внешнего: запись префаба из
// него - цикл, она выпадает с ошибкой в лог, как и запись префаба, который не
// развернулся. nullopt, если сама branch - такая запись. В used (если он
// задан) дописываются префабы, из которых ветка собрана.
YQLORA_API std::optional<nlohmann::json> resolvePrefabs(
    nlohmann::json branch,
    const PrefabSources& sources,
    const TypeRegistry& registry,
    std::vector<ResourceId>& chain,
    std::vector<ResourceId>* used = nullptr
);

struct ExpandedPrefab {
    // Ветка экземпляра: узлы base с выведенными ID и ссылками, переопределения
    // записи применены. Записи экземпляров среди добавленных узлов остаются
    // записями.
    nlohmann::json tree;
    // Переопределения записи, чьей цели в base нет, в формате записи.
    nlohmann::json unused;
};

// base - дерево префаба без записей (resolvePrefabs), record - запись
// экземпляра instance.
YQLORA_API ExpandedPrefab
expandPrefab(const nlohmann::json& base, const nlohmann::json& record, NodeId instance, const TypeRegistry& registry);

// Переопределения в tree (дерево префаба в его пространстве) по записи. Свойство
// - значением целиком, удалённые узлы и компоненты убираются, добавленные
// дописываются: узел - последним ребёнком. Возвращает переопределения без
// цели в формате записи.
YQLORA_API nlohmann::json applyOverrides(nlohmann::json& tree, const nlohmann::json& record);

// Запись экземпляра instance префаба prefab: live - его ветка, как её пишет
// сцена (ID экземпляра, вложенные экземпляры записями), base - дерево, из
// которого он развёрнут; unused дописываются к найденным переопределениям.
// Имя корня и значения его встроенных компонентов (положение) - всегда свои,
// они пишутся, даже если совпадают с префабом.
YQLORA_API nlohmann::json computeOverrides(
    const nlohmann::json& base,
    nlohmann::json live,
    ResourceId prefab,
    NodeId instance,
    const nlohmann::json& unused,
    const TypeRegistry& registry
);

// Узлы экземпляра instance, развёрнутого из base: ID в сцене -> ID в префабе.
YQLORA_API std::unordered_map<NodeId, NodeId> instanceSources(const nlohmann::json& base, NodeId instance);

// Новые ID экземпляру записи и узлам и компонентам, добавленным в него: копия
// рядом с оригиналом. Выведенные ID следуют за ID экземпляра сами.
YQLORA_API void freshenRecord(nlohmann::json& record);

} // namespace yqlora
