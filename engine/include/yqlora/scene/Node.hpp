#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Layers.hpp"
#include "yqlora/scene/NodeId.hpp"
#include "yqlora/scene/PrefabLink.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

class Scene;
struct PrefabInstance;

// Сохранять ли положение узла в мире, когда он переходит к другому родителю.
// No - значения относительно родителя те же, узел прыгает вместе с ними
// (меч в руку); Yes - пересчитываются, узел остаётся на месте (вагон к поезду).
enum class KeepWorld : bool {
    No,
    Yes,
};

// Базовый элемент дерева сцены. Родитель владеет детьми, ребёнок хранит
// невладеющий указатель на родителя. Имена и типы всегда на английском.
//
// Сам Node не имеет положения, это делают пространственные узлы (Node3D), у которых
// Transform есть всегда. Остальные компоненты подключаются по необходимости.
class YQLORA_API Node {
public:
    explicit Node(std::string name = "Node");
    virtual ~Node();

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    // Корень иерархии: свой typeName без override, поэтому без YQLORA_TYPE.
    // Базы в реестре нет.
    using YqloraThisType = Node;
    using YqloraBaseType = void;

    static constexpr std::string_view staticTypeName()
    {
        return "Node";
    }

    virtual std::string_view typeName() const
    {
        return staticTypeName();
    }

    // Уникален и не меняется за жизнь узла: по нему узлы ссылаются друг
    // на друга в файлах сцен и в редакторе. Выдаётся случайно при создании.
    NodeId id() const
    {
        return m_id;
    }

    // Узел экземпляра префаба: из какого экземпляра и узла префаба он
    // взялся. Пусто у узлов не из префаба и у добавленных в экземпляр.
    const PrefabLink& prefabLink() const
    {
        return m_prefabLink;
    }

    // Корень экземпляра префаба: префаб, ID экземпляра и его переопределения
    // (PrefabInstance.hpp). nullptr у других узлов.
    const PrefabInstance* prefabInstance() const
    {
        return m_prefabInstance.get();
    }

    const std::string& name() const
    {
        return m_name;
    }

    void setName(std::string name)
    {
        m_name = std::move(name);
    }

    Node* parent() const
    {
        return m_parent;
    }

    // Неактивный узел выключает всю свою ветку: компоненты в ней не участвуют
    // в игре, не рисуются и не выбираются мышью.
    bool active() const
    {
        return m_active;
    }

    void setActive(bool active);

    // Узел и все его предки активны.
    bool activeInHierarchy() const
    {
        return m_activeInHierarchy;
    }

    // Слой узла от 0 до kLayerCount - 1: по нему фильтруют маски камер и физика.
    int layer() const
    {
        return m_layer;
    }

    void setLayer(int layer);

    // Группы узла ("enemies", "pickups"): по ним сцена отдаёт узлы без обхода
    // дерева (Scene::group). Список хранится как задан (в инспекторе новая
    // строка сначала пуста); пустое имя - не группа, повтор не считается.
    const std::vector<std::string>& groups() const
    {
        return m_groups;
    }

    void setGroups(std::vector<std::string> groups);
    void addToGroup(std::string_view group);
    void removeFromGroup(std::string_view group);
    bool isInGroup(std::string_view group) const;

    // Сцена, в дереве которой узел. nullptr, пока узел вне сцены.
    Scene* scene() const
    {
        return m_scene;
    }

    std::span<const std::unique_ptr<Node>> children() const
    {
        return m_children;
    }

    // Создаёт узел типа T и делает его последним ребёнком.
    template <std::derived_from<Node> T, typename... Args>
    T& addChild(Args&&... args)
    {
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *node;
        addChild(std::move(node));

        return ref;
    }

    Node& addChild(std::unique_ptr<Node> child);

    // Вставляет ребёнка на место index среди детей (не больше их числа),
    // например вернуть удалённый узел туда, где он был.
    Node& insertChild(std::size_t index, std::unique_ptr<Node> child);

    // Отцепляет ребёнка и возвращает владение им; nullptr, если это не наш ребёнок.
    // Во время кадра сцены нельзя: там Scene::destroy и Scene::setParent.
    std::unique_ptr<Node> removeChild(Node& child);

    // Поиск среди прямых детей по имени.
    Node* findChild(std::string_view name) const;

    // Узел по пути от этого: имена детей через "/", ".." - родитель, пустой
    // путь - сам узел ("Arm/Hand", "../Door"). nullptr, если пути нет.
    Node* find(std::string_view path) const;

    std::span<const std::unique_ptr<Component>> components() const
    {
        return m_components;
    }

    // Создаёт компонент типа T и подключает к узлу.
    template <std::derived_from<Component> T, typename... Args>
    T& addComponent(Args&&... args)
    {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *component;
        addComponent(std::move(component));

        return ref;
    }

    // В сцене проверяет правила компонентов (ComponentRules.hpp): недостающие
    // обязательные компоненты добавляет перед ним сам, а нарушение правил
    // (не тот узел, второй такой же) - ошибка программы.
    Component& addComponent(std::unique_ptr<Component> component);

    // Вставляет компонент на место index в списке (не больше его длины).
    Component& insertComponent(std::size_t index, std::unique_ptr<Component> component);

    // Первый компонент типа T (или его наследника); nullptr, если такого нет.
    template <std::derived_from<Component> T>
    T* getComponent() const
    {
        for (const auto& c : m_components) {
            if (auto* typed = dynamic_cast<T*>(c.get())) {
                return typed;
            }
        }

        return nullptr;
    }

    // Все компоненты типа T (или его наследников) в порядке узла: для типов,
    // которых на узле бывает несколько (allowMultiple).
    template <std::derived_from<Component> T>
    std::vector<T*> getComponents() const
    {
        std::vector<T*> found;
        for (const auto& c : m_components) {
            if (auto* typed = dynamic_cast<T*>(c.get())) {
                found.push_back(typed);
            }
        }

        return found;
    }

    // Первый компонент типа T у узла или его потомков, в глубину, сам узел
    // первым; неактивные ветки тоже. nullptr, если нет нигде.
    template <std::derived_from<Component> T>
    T* findComponentInChildren() const
    {
        if (T* own = getComponent<T>()) {
            return own;
        }
        for (const auto& child : m_children) {
            if (T* found = child->findComponentInChildren<T>()) {
                return found;
            }
        }

        return nullptr;
    }

    // Первый компонент типа T у узла или его предков, от узла к корню.
    template <std::derived_from<Component> T>
    T* findComponentInParent() const
    {
        for (const Node* node = this; node; node = node->m_parent) {
            if (T* found = node->getComponent<T>()) {
                return found;
            }
        }

        return nullptr;
    }

    // Компонент наш и не встроенный.
    bool canRemoveComponent(const Component& component) const;

    // Отключает компонент и возвращает владение им; nullptr, если снять нельзя
    // (см. canRemoveComponent).
    std::unique_ptr<Component> removeComponent(Component& component);

protected:
    // Для пространственных узлов (Node3D): подключает компонент, который нельзя снять.
    Component& addBuiltinComponent(std::unique_ptr<Component> component);

    virtual void onParentChanged()
    {
    }

    // Узел переносят с KeepWorld::Yes. Узел с положением запоминает своё
    // мировое положение и возвращает, как вернуть его после переноса; узлу без
    // положения возвращать нечего.
    virtual std::function<void()> captureWorldPosition()
    {
        return {};
    }

    // Узел попал в дерево сцены или ушёл из него, см. Component::onEnterScene.
    // При входе сначала узел, потом дети; при выходе наоборот.
    virtual void onEnterScene()
    {
    }

    virtual void onExitScene()
    {
    }

private:
    // ID из файла сцены и связь с префабом выставляет только загрузчик.
    friend class SceneSerializer;
    void setId(NodeId id);
    void setPrefabLink(PrefabLink link);
    void setPrefabInstance(std::unique_ptr<PrefabInstance> instance);

    // Вход в сцену и выход и остановку вызывает сама сцена для корня,
    // остальное - узлы по дереву. Кадр игры идёт не по дереву, а по спискам
    // фаз сцены (Scene::advance).
    friend class Scene;
    void enterScene(Scene& scene);
    void exitScene();

    // Переносит узел последним ребёнком parent той же сцены, не выходя из неё:
    // компоненты не останавливаются. Проверки - в Scene::setParent.
    void moveTo(Node& parent, KeepWorld keepWorld);

    // Игра остановлена: onDisable и onStop у компонентов узла и детей, которые
    // их ждут. Следующий update снова начнёт с onEnable и onStart.
    void stop();

    // Пересчитывает activeInHierarchy после смены флага или родителя, в сцене
    // включает и выключает компоненты ветки.
    void updateActiveInHierarchy();

    void addRequiredComponents(const Component& component);

    NodeId m_id;
    PrefabLink m_prefabLink;
    std::unique_ptr<PrefabInstance> m_prefabInstance;
    std::string m_name;
    Node* m_parent = nullptr;
    Scene* m_scene = nullptr;
    bool m_active = true;
    bool m_activeInHierarchy = true;
    int m_layer = 0;
    std::vector<std::string> m_groups;
    std::vector<std::unique_ptr<Component>> m_components;
    std::vector<std::unique_ptr<Node>> m_children;
};

} // namespace yqlora
