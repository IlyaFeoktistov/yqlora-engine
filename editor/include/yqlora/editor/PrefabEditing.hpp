#pragma once

#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/NodeId.hpp"
#include "yqlora/scene/ObjectId.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string_view>

namespace yqlora {
class Component;
class Node;
class Property;
struct PrefabInstance;
class TypeRegistry;
} // namespace yqlora

namespace yqlora::editor {

class EditorSession;

// Корень экземпляра префаба, к которому относится узел: сам узел, если он
// корень, иначе ближайший предок-корень. Узел внутри - из префаба или
// добавленный в экземпляр. nullptr - узел вне экземпляров.
Node* instanceRootOf(Node& node);
const Node* instanceRootOf(const Node& node);

// Чем экземпляр отличается от префаба, для отметок в инспекторе. Снимок на
// момент построения: запись экземпляра считается здесь.
class InstanceOverrides {
public:
    // root - корень экземпляра (instanceRootOf).
    InstanceOverrides(const Node& root, const TypeRegistry& registry);

    // Свойство узла (component - nullptr) или его компонента переопределено.
    bool property(const Node& node, const Component* component, std::string_view property) const;

    // Узел или компонент добавлен в экземпляр, в префабе его нет.
    bool addedNode(const Node& node) const;
    bool addedComponent(const Node& node, const Component& component) const;

    // У компонента префаба есть переопределённые свойства.
    bool componentOverridden(const Node& node, const Component& component) const;

    // Есть переопределения, кроме своих значений корня (имени и положения).
    bool any() const;

    // Переопределения без цели (см. prefabs.md).
    std::size_t unused() const;

private:
    // ID компонента в префабе; 0 - компонент добавлен.
    ComponentId sourceOf(const Node& node, const Component& component) const;

    const Node& m_root;
    const PrefabInstance& m_instance;
    const TypeRegistry& m_registry;
    nlohmann::json m_record;
};

// Работа с экземплярами префабов в открытом документе: вернуть
// переопределения, применить их к префабу, распаковать, сохранить ветку
// префабом, перестроить экземпляры изменившихся префабов. Каждое действие -
// один шаг истории; во время игры ничего не делается. Без интерфейса.
class PrefabEditing {
public:
    explicit PrefabEditing(EditorSession& session)
        : m_session(session)
    {
    }

    // Экземпляр prefab можно поставить в открытый документ: если документ -
    // префаб, prefab его не содержит (прямо или через другие), иначе был бы
    // цикл. Сцена не префаб - это проверяет само развёртывание.
    bool canInstantiate(ResourceId prefab) const;

    // Вернуть значение префаба свойству узла экземпляра (component - nullptr
    // для свойства узла).
    bool revertProperty(Node& node, Component* component, const Property& property);
    // Компонент префаба - все его свойства; добавленный - убрать.
    bool revertComponent(Node& node, Component& component);
    // Все переопределения экземпляра, кроме своих значений корня и
    // неиспользуемых.
    bool revertInstance(Node& root);
    bool removeUnused(Node& root);

    // Префаб экземпляра - свой файл .yprefab проекта: в него можно применять.
    // Модель и встроенный меш только читаются.
    bool canApply(const Node& root) const;

    // Пишет переопределение в файл префаба; все экземпляры в документе
    // перестраиваются, у этого оно перестаёт быть переопределением.
    bool applyProperty(Node& node, Component* component, const Property& property);
    bool applyComponent(Node& node, Component& component);
    // Все переопределения, кроме своих значений корня и неиспользуемых.
    // Добавленный узел, содержащий префаб экземпляра, был бы циклом - отказ.
    bool applyInstance(Node& root);

    // Экземпляр становится обычными узлами с теми же ID.
    bool unpack(Node& root);

    // Пишет ветку node в новый префаб name.yprefab в папке folder проекта и
    // ставит на её место экземпляр; ссылки сцены на узлы ветки переводятся на
    // узлы экземпляра. Файл остаётся и после отмены, как любой созданный.
    // Новый корень выбирается; nullptr, если имя не годится или занято.
    Node* saveAsPrefab(Node& node, const std::filesystem::path& folder, std::string_view name);

    // Экземпляры в документе, чьи префабы изменились (файл перечитан, типы
    // поменялись) или впервые развернулись, строятся заново из их записей.
    // Не правка: в историю не попадает. true, если что-то перестроено.
    bool rebuildChanged();

private:
    // Переопределение записи по виду ("overrides", "added", ...) и самому себе.
    using EntryFilter = std::function<bool(std::string_view kind, const nlohmann::json& entry)>;

    // Переопределения экземпляра root, выбранные select (кроме своих
    // значений корня и неиспользуемых), - в файл префаба.
    bool applyEntries(Node& root, const EntryFilter& select);

    EditorSession& m_session;
};

} // namespace yqlora::editor
