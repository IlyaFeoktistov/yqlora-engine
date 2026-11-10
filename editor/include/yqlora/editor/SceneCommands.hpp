#pragma once

#include "yqlora/editor/Command.hpp"
#include "yqlora/editor/Selection.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/Transform3D.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace yqlora {

class TypeInfo;

} // namespace yqlora

namespace yqlora::editor {

// Узел или компонент узла, чьё свойство меняют. Компонент задан местом
// в списке: после Stop компоненты пересоздаются в том же порядке.
struct PropertyTarget {
    NodeId node = 0;
    // nullopt - свойство самого узла.
    std::optional<std::size_t> component;

    bool operator==(const PropertyTarget&) const = default;
};

// Ветка дерева, вынутая из сцены, и место, куда её вернуть.
struct DetachedBranch {
    std::unique_ptr<Node> node;
    // 0 - ветка была корнем сцены.
    NodeId parent = 0;
    std::size_t index = 0;
};

// Новая ветка (узел или целое дерево, например модель из файла) ребёнком
// parent (0 - ребёнком корня, а в пустой сцене - корнем). Имя её корня
// уникально среди соседей. Корень ветки выбирается.
class AddBranchCommand final : public Command {
public:
    // Ветка готова заранее, поэтому ID её корня известен до apply.
    AddBranchCommand(std::unique_ptr<Node> branch, NodeId parent);

    bool apply(EditorSession& session) override;
    void revert(EditorSession& session) override;

    // 0, если ветки нет.
    NodeId created() const
    {
        return m_created;
    }

private:
    NodeId m_parent;
    NodeId m_created = 0;
    bool m_placed = false;
    Selection m_previousSelection;
    DetachedBranch m_branch;
};

// Удаление узла со всеми детьми. Выделение внутри ветки при отмене
// возвращается.
class DeleteNodeCommand final : public Command {
public:
    explicit DeleteNodeCommand(NodeId node);

    bool apply(EditorSession& session) override;
    void revert(EditorSession& session) override;

private:
    NodeId m_node;
    Selection m_previousSelection;
    DetachedBranch m_branch;
};

class AddComponentCommand final : public Command {
public:
    AddComponentCommand(NodeId node, const TypeInfo& type);

    bool apply(EditorSession& session) override;
    void revert(EditorSession& session) override;

private:
    NodeId m_node;
    const TypeInfo& m_type;
    std::optional<std::size_t> m_index;
    std::unique_ptr<Component> m_detached;
};

// Несколько команд одним шагом истории, например удаление нескольких узлов.
// Делаются по порядку, отменяются в обратном. Если одна не удалась,
// сделанные до неё отменяются и вся команда не удаётся.
class CompositeCommand final : public Command {
public:
    explicit CompositeCommand(std::vector<std::unique_ptr<Command>> commands);

    bool apply(EditorSession& session) override;
    void revert(EditorSession& session) override;

private:
    std::vector<std::unique_ptr<Command>> m_commands;
};

// Встроенный компонент (Transform) снять нельзя.
class RemoveComponentCommand final : public Command {
public:
    RemoveComponentCommand(NodeId node, std::size_t index);

    bool apply(EditorSession& session) override;
    void revert(EditorSession& session) override;

private:
    NodeId m_node;
    std::size_t m_index;
    std::unique_ptr<Component> m_detached;
};

// Правки одного свойства сливаются, пока не вызван finishEdit.
class SetPropertyCommand final : public Command {
public:
    SetPropertyCommand(PropertyTarget target, const Property& property, PropertyValue value);

    bool apply(EditorSession& session) override;
    void revert(EditorSession& session) override;
    bool merge(const Command& next) override;

private:
    PropertyTarget m_target;
    const Property& m_property;
    PropertyValue m_value;
    std::optional<PropertyValue> m_previous;
};

// Положение, поворот и масштаб разом, например от манипулятора.
class SetTransformCommand final : public Command {
public:
    SetTransformCommand(NodeId node, const TransformValues& values);

    bool apply(EditorSession& session) override;
    void revert(EditorSession& session) override;
    bool merge(const Command& next) override;

private:
    NodeId m_node;
    TransformValues m_values;
    std::optional<TransformValues> m_previous;
};

// Перенос узла последним ребёнком другого узла. keepWorld решает, что
// сохранить: мировое положение или локальные значения (см. Scene::setParent).
// Отмена возвращает и родителя с местом среди соседей, и прежние локальные
// значения трансформа.
class SetParentCommand final : public Command {
public:
    SetParentCommand(NodeId node, NodeId parent, KeepWorld keepWorld);

    bool apply(EditorSession& session) override;
    void revert(EditorSession& session) override;

private:
    NodeId m_node;
    NodeId m_parent;
    KeepWorld m_keepWorld;
    NodeId m_previousParent = 0;
    std::size_t m_previousIndex = 0;
    std::optional<TransformValues> m_previousValues;
};

// Файл префаба, который правка переписывает (применение переопределений к
// префабу): отмена возвращает прежнее содержимое.
struct PrefabFileChange {
    ResourceId prefab = 0;
    // Полный путь к файлу.
    std::filesystem::path file;
    nlohmann::json after;
};

// Правка, которую проще описать действием, чем обратимыми шагами
// (экземпляры префабов: вернуть, применить, распаковать): сцена до и после
// неё запоминается целиком, отмена и повтор ставят нужную. Узлы находятся
// по ID, поэтому остальная история работает и через неё. Если задан файл
// префаба, он пишется до правки и возвращается при отмене, а кэш ресурсов
// перечитывает его.
class SceneChangeCommand final : public Command {
public:
    // Делает правку в первый раз; false - не вышло, сцена возвращается.
    using Change = std::function<bool(EditorSession&)>;

    explicit SceneChangeCommand(Change change, std::optional<PrefabFileChange> file = std::nullopt);

    bool apply(EditorSession& session) override;
    void revert(EditorSession& session) override;

private:
    Change m_change;
    std::optional<PrefabFileChange> m_file;
    nlohmann::json m_fileBefore;
    std::string m_before;
    std::string m_after;
    bool m_done = false;
};

// Можно ли перенести node под parent: не корень, не в свою же ветку, не к
// тому же родителю и не узел префаба внутри экземпляра.
bool canMoveUnder(const Node& node, const Node& parent);

} // namespace yqlora::editor
