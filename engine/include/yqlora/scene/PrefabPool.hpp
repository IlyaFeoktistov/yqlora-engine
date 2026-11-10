#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/reflection/TypeName.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Prefab.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace yqlora {

class Node;

// Что пул делает, когда свободных экземпляров нет.
enum class PoolWhenEmpty : std::uint8_t {
    // Создаёт ещё один (развёртывание на ходу, как instantiate).
    Grow,
    // Ничего не выдаёт: acquire возвращает nullptr.
    Refuse,
};

// Пул экземпляров префаба для объектов, которые появляются и исчезают часто
// (пули, гильзы, эффекты попаданий): экземпляры создаются заранее, в начале
// игры, и живут выключенными под узлом пула. acquire выдаёт свободный,
// release возвращает его - без развёртывания и уничтожения на ходу.
//
// Возвращённый экземпляр получает значения префаба (свойства узлов и
// компонентов, положение корня), так что выданный - как новый. Состояние,
// которого нет в свойствах (поля скрипта без описания), скрипт сбрасывает
// сам в onEnable или onDisable: они приходят, как при любом включении.
// Экземпляр, чьё дерево за жизнь изменилось (узлы или компоненты добавлены
// или убраны), при возврате заменяется новым.
class YQLORA_API PrefabPool final : public Component {
public:
    YQLORA_TYPE(PrefabPool, Component)

    PrefabPool();
    ~PrefabPool() override;

    Ref<Prefab> prefab;
    // Сколько экземпляров создаётся в начале игры.
    int size = 0;
    PoolWhenEmpty whenEmpty = PoolWhenEmpty::Grow;

    // Свободный экземпляр, включённый, последним ребёнком parent (во время
    // кадра переносится в его конце, как Scene::setParent; положение корня -
    // относительно parent). nullptr, если свободных нет и пул не растёт или
    // префаб не развернулся.
    Node* acquire(Node& parent);
    Node* acquire(const NodeRef& parent);

    // Возвращает выданный экземпляр (его корень) в пул. false с сообщением в
    // лог, если узел не выдан этим пулом.
    bool release(Node& node);
    bool release(const NodeRef& node);

    // Свободных экземпляров и всех, созданных пулом и ещё живых.
    int available() const;
    int total() const;

    void onStart() override;

private:
    struct Slot;

    // Экземпляры size, если ещё не созданы: и до onStart пула, если кто-то
    // берёт из него раньше.
    void fill();
    // Новый свободный экземпляр под узлом пула; false, если префаб не развернулся.
    bool create();
    // Слоты экземпляров, которых уже нет в сцене, убираются.
    void forgetDestroyed();

    std::vector<Slot> m_slots;
    bool m_filled = false;
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::PoolWhenEmpty, "Grow", "Refuse")
