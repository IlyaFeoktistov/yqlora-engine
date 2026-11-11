#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/TypeName.hpp"

#include <string_view>

namespace yqlora {

class Scene;

// Подсистема сцены: физика, звук, рендер, трансформы. Главная точка
// расширения: модуль регистрирует систему и её компоненты, а Scene, цикл
// кадра и приложение не меняются.
//
//   registry.registerSystem<PhysicsWorld>();     // конструктор PhysicsWorld(Scene&)
//   scene.system<PhysicsWorld>()                 // компонент регистрирует тело в onEnterScene
//
// Каждая сцена создаёт по экземпляру каждой зарегистрированной системы, в
// порядке регистрации. Сцена редактора тоже, но шаги кадра (advance) делает
// только копия Play.
class YQLORA_API SceneSystem {
public:
    explicit SceneSystem(Scene& scene)
        : m_scene(scene)
    {
    }

    virtual ~SceneSystem();

    SceneSystem(const SceneSystem&) = delete;
    SceneSystem& operator=(const SceneSystem&) = delete;

    // Корень иерархии систем, как Node у узлов: typeName чисто виртуальный,
    // поэтому без YQLORA_TYPE. Базы в реестре нет.
    using YqloraThisType = SceneSystem;
    using YqloraBaseType = void;

    static constexpr std::string_view staticTypeName()
    {
        return "SceneSystem";
    }

    virtual std::string_view typeName() const = 0;

    Scene& scene() const
    {
        return m_scene;
    }

    // Шаги кадра, порядок - в Scene::advance. fixedUpdate - перед
    // onFixedUpdate компонентов, afterFixedUpdate - после него (шаг физики:
    // силы, добавленные скриптами в onFixedUpdate, действуют в том же шаге),
    // update - после onUpdate, lateUpdate - после onLateUpdate (список
    // отрисовки, звук).
    virtual void fixedUpdate(float /*stepSeconds*/)
    {
    }

    virtual void afterFixedUpdate(float /*stepSeconds*/)
    {
    }

    virtual void update(float /*deltaSeconds*/)
    {
    }

    virtual void lateUpdate(float /*deltaSeconds*/)
    {
    }

    // После симуляции и отложенных изменений дерева, перед Extract: мировые
    // матрицы изменённых узлов. Вызывается и в редакторе, без advance.
    virtual void updateTransforms()
    {
    }

private:
    Scene& m_scene;
};

} // namespace yqlora
