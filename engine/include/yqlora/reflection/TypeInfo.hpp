#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/EventInfo.hpp"
#include "yqlora/reflection/Method.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/scene/LifecyclePhase.hpp"

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace yqlora {

class Component;
class Node;
class Scene;
class SceneSystem;
class SettingsSection;

enum class TypeKind {
    Node,
    Component,
    System,
    Settings,
};

// Описание типа узла, компонента, системы сцены или раздела настроек: по нему редактор строит меню создания
// и инспектор, а файлы сцен создают объекты по имени типа.
class YQLORA_API TypeInfo {
public:
    const std::string& name() const
    {
        return m_name;
    }

    // Пусто у Node, Component и SceneSystem: это корни иерархий.
    const std::string& baseName() const
    {
        return m_baseName;
    }

    TypeKind kind() const
    {
        return m_kind;
    }

    // Прежние имена типа: файл с таким именем читается (TypeRegistry::find), пишется уже новое.
    std::span<const std::string> formerNames() const
    {
        return m_formerNames;
    }

    // Модуль, который зарегистрировал тип: движок или dll игры. Типы модуля
    // убираются из реестра вместе, когда dll выгружают.
    const std::string& module() const
    {
        return m_module;
    }

    // Встроенные компоненты (Transform) создаёт сам узел, поэтому они не создаваемые.
    bool creatable() const
    {
        return m_createNode != nullptr || m_createComponent != nullptr || m_createSystem != nullptr
               || m_createSettings != nullptr;
    }

    // Встроенный компонент: его создаёт сам узел, снять и выключить его нельзя.
    // Это несоздаваемые компоненты, кроме корня Component.
    bool builtin() const
    {
        return m_kind == TypeKind::Component && !creatable() && !m_baseName.empty();
    }

    // Ключ перевода с описанием типа для подсказки в редакторе.
    std::string descriptionKey() const;

    // Только свойства этого типа, без базовых. Все вместе: TypeRegistry::properties().
    std::span<const Property> ownProperties() const
    {
        return m_properties;
    }

    // Только методы этого типа, без базовых. Все вместе: TypeRegistry::methods().
    std::span<const Method> ownMethods() const
    {
        return m_methods;
    }

    // Только события этого типа, без базовых. Все вместе: TypeRegistry::events().
    std::span<const EventInfo> ownEvents() const
    {
        return m_events;
    }

    // Правила компонента, только свои, без базовых. Проверяют их функции
    // из ComponentRules.hpp, они же учитывают правила базовых типов.

    // Тип узла, на котором может жить компонент; пусто - на любом.
    const std::string& requiredNode() const
    {
        return m_requiredNode;
    }

    // Компоненты, которые должны быть на том же узле.
    std::span<const std::string> requiredComponents() const
    {
        return m_requiredComponents;
    }

    // Можно несколько компонентов этого типа на одном узле.
    bool allowsMultiple() const
    {
        return m_allowMultiple;
    }

    // Фазы кадра, которые переопределил тип компонента (см. LifecyclePhase.hpp).
    LifecyclePhases phases() const
    {
        return m_phases;
    }

    // Тип - скрипт: поведение, написанное для игры (C++, Luau, граф).
    // "Добавить компонент" собирает скрипты по этой пометке, а не по базовому классу.
    bool isScript() const
    {
        return m_script;
    }

    // Порядок выполнения по умолчанию из кода. Действующий порядок - executionOrderOf.
    int executionOrder() const
    {
        return m_executionOrder;
    }

    // Имя файла раздела настроек без расширения: settings/<имя>.json в папке
    // проекта. Пусто у остальных видов типов.
    const std::string& settingsFile() const
    {
        return m_settingsFile;
    }

    // nullptr, если это не узел (не компонент) или тип нельзя создать.
    std::unique_ptr<Node> createNode() const;
    std::unique_ptr<Component> createComponent() const;
    std::unique_ptr<SceneSystem> createSystem(Scene& scene) const;
    std::unique_ptr<SettingsSection> createSettings() const;

private:
    friend class TypeRegistry;
    template <typename Self>
    friend class BasicTypeBuilder;
    friend class TypeBuilder;

    std::string m_name;
    std::string m_baseName;
    std::vector<std::string> m_formerNames;
    std::string m_module;
    TypeKind m_kind = TypeKind::Node;
    std::function<std::unique_ptr<Node>()> m_createNode;
    std::function<std::unique_ptr<Component>()> m_createComponent;
    std::function<std::unique_ptr<SceneSystem>(Scene&)> m_createSystem;
    std::function<std::unique_ptr<SettingsSection>()> m_createSettings;
    std::string m_settingsFile;
    std::vector<Property> m_properties;
    std::vector<Method> m_methods;
    std::vector<EventInfo> m_events;
    std::string m_requiredNode;
    std::vector<std::string> m_requiredComponents;
    bool m_allowMultiple = false;
    bool m_script = false;
    // Тип - C++-класс (TypeRegistry::find<T>), а не описанный во время выполнения.
    bool m_cppClass = false;
    LifecyclePhases m_phases = 0;
    int m_executionOrder = 0;
};

} // namespace yqlora
