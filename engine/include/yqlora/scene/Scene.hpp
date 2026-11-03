#pragma once

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/scene/ComponentIndex.hpp"
#include "yqlora/scene/ComponentLifecycle.hpp"
#include "yqlora/scene/GameTime.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/SceneSystems.hpp"
#include "yqlora/scene/TreeChangeQueue.hpp"

#include <concepts>
#include <memory>
#include <ranges>
#include <span>
#include <string_view>

namespace yqlora {

// Дерево узлов с одним корнем. Новая сцена пуста:
// корня нет, пока его не создадут (любого типа: Node, Node3D, ...).
class YQLORA_API Scene {
public:
    // Службы задаёт владелец сцены и они переживают её; см. EngineServices::none().
    // Системы сцены создаются здесь, по одной на каждый тип в реестре.
    explicit Scene(const EngineServices& services);
    ~Scene();

    // Узлы сцены хранят указатель на неё, поэтому сцену не копируют и не перемещают.
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    const EngineServices& services() const
    {
        return m_services;
    }

    bool empty() const
    {
        return !m_root;
    }

    // nullptr, если сцена пуста.
    Node* root()
    {
        return m_root.get();
    }

    const Node* root() const
    {
        return m_root.get();
    }

    // Создаёт корень типа T. Прежний корень со всем деревом уничтожается.
    template <std::derived_from<Node> T, typename... Args>
    T& createRoot(Args&&... args)
    {
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *node;
        setRoot(std::move(node));

        return ref;
    }

    // Делает узел корнем (у него не должно быть родителя). Прежний корень
    // уничтожается; nullptr очищает сцену.
    void setRoot(std::unique_ptr<Node> root);

    // Забирает дерево из сцены, сцена становится пустой. Запущенное в игре останавливается.
    std::unique_ptr<Node> takeRoot();

    // Уничтожает узел этой сцены со всей веткой, корень тоже. Во время кадра
    // (см. inFrame) - в его конце, чтобы ни одна фаза не видела наполовину
    // изменённое дерево; вне кадра - сразу.
    void destroy(Node& node);

    // Переносит узел последним ребёнком parent той же сцены, компоненты не
    // останавливаются. Что станет с положением, решает keepWorld: умолчания
    // нет, любое кого-то удивило бы. Когда применяется - как у
    // destroy. Корень и перенос внутрь своей же ветки - ошибка программы.
    void setParent(Node& node, Node& parent, KeepWorld keepWorld);

    // Идёт кадр (advance): изменения дерева ждут его конца, а компоненты,
    // включённые в нём (и узлы, созданные в нём), участвуют со следующего.
    bool inFrame() const
    {
        return m_lifecycle.inFrame();
    }

    // Компоненты в дереве сцены ровно этого типа (без наследников), в порядке
    // попадания в сцену. Так подсистемы находят свои компоненты без обхода дерева.
    template <std::derived_from<Component> T>
    auto components() const
    {
        return componentsOfType(typeNameOf<T>()) | std::views::transform([](Component* component) {
                   return static_cast<T*>(component);
               });
    }

    // По имени типа: так находятся и типы, описанные во время выполнения, у
    // которых нет своего C++-класса (все скрипты Luau - один LuauScript).
    std::span<Component* const> componentsOfType(std::string_view typeName) const;

    // Система сцены типа T; nullptr, если такая не зарегистрирована.
    template <std::derived_from<SceneSystem> T>
    T* system() const
    {
        return m_systems.get<T>();
    }

    // Мировые матрицы изменённых узлов, см. SceneSystem::updateTransforms.
    // Цикл кадра вызывает после advance, редактор - перед отрисовкой.
    void updateTransforms();

    GameTime& time()
    {
        return m_time;
    }

    const GameTime& time() const
    {
        return m_time;
    }

    // Кадр игры длиной realDelta секунд по часам. Весь порядок кадра - здесь:
    // onEnable и onStart новым компонентам, фиксированные шаги, update,
    // lateUpdate (системы - рядом с компонентами, см. SceneSystem), затем
    // отложенные изменения дерева. Вызывают только участвующие компоненты (activeAndEnabled),
    // у типа которых эта фаза переопределена. Исключение из вызова жизненного
    // цикла выключает компонент и уходит в лог, кадр идёт дальше.
    void advance(float realDelta);

    // Игра остановлена: onDisable и onStop у всего, что успело запуститься.
    void stop();

private:
    friend class Node;
    friend class Component;
    // Компонент вошёл в дерево сцены или вышел из него: индекс по типам
    // и, если он участвует в игре, списки фаз.
    void enterComponent(Component& component);
    void exitComponent(Component& component);

    // Участие компонента в игре, см. ComponentLifecycle.
    void activate(Component& component);
    void deactivate(Component& component);
    void stopComponent(Component& component);

    void destroyNow(Node& node);

    const EngineServices& m_services;
    GameTime m_time;
    ComponentLifecycle m_lifecycle{m_services};
    ComponentIndex m_components;
    TreeChangeQueue m_treeChanges;
    std::unique_ptr<Node> m_root;
    // Создаются последними: конструктор системы может обратиться к сцене.
    // Дерево к их уничтожению уже пусто (см. ~Scene), так что компоненты,
    // выходя из сцены, системы ещё застают.
    SceneSystems m_systems{*this, m_services.registry()};
};

} // namespace yqlora
