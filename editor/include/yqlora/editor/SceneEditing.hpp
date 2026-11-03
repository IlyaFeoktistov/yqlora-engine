#pragma once

#include "yqlora/editor/Command.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/NodeId.hpp"
#include "yqlora/scene/Transform3D.hpp"

#include <memory>
#include <span>

namespace yqlora {
class MeshRenderer;
struct Ray;
class Camera3D;
class Component;
class Node;
class Node3D;
class TypeInfo;
} // namespace yqlora

namespace yqlora::editor {

class EditorSession;

// Правка текущей сцены сессии командами с отменой: узлы, компоненты,
// свойства, трансформы. Методы ниже execute - короткие пути к командам из
// SceneCommands.hpp. Отмена и повтор - у сессии, история общая с настройками.
class SceneEditing {
public:
    explicit SceneEditing(EditorSession& session)
        : m_session(session)
    {
    }

    // Во время игры правка в историю не попадает: по Stop сцена всё равно вернётся как была.
    bool execute(std::unique_ptr<Command> command);

    // Создаёт узел типа type ребёнком parent. В пустой сцене узел становится
    // корнем, а без parent - ребёнком корня. Имя уникально среди соседей.
    // Новый узел выбирается. nullptr, если тип не узел или его нельзя создать.
    Node* createNode(const TypeInfo& type, Node* parent);

    // Готовая ветка ребёнком parent, по тем же правилам, что createNode.
    // nullptr, если ветки нет.
    Node* addBranch(std::unique_ptr<Node> branch, Node* parent);

    // Ставит ресурс проекта в сцену ребёнком parent одним шагом истории:
    // модель - деревом её узлов, меш - узлом с MeshRenderer (см.
    // instantiateResource). nullptr, если ресурс не модель и не меш.
    Node* addResource(ResourceId resource, Node* parent);

    // То же, с корнем новой ветки в точке мира worldPosition.
    Node* addResource(ResourceId resource, Node* parent, const Vec3& worldPosition);

    // Удаляет узел со всеми детьми. Выделение внутри удалённой ветки
    // сбрасывается, чтобы на него не осталось висячих указателей.
    bool deleteNode(NodeId id);

    // Удаляет несколько узлов одним шагом истории. Узлы внутри других
    // удаляемых веток пропускаются: они уходят вместе с веткой.
    bool deleteNodes(std::span<const NodeId> ids);

    // Добавляет компонент вместе с недостающими обязательными одним шагом
    // истории. nullptr, если тип нельзя создать или он не подходит узлу
    // (см. canAttach и missingRequirements в ComponentRules.hpp).
    Component* addComponent(Node& node, const TypeInfo& type);

    // Снимает компонент, если его можно снять (canDetach в ComponentRules.hpp).
    bool removeComponent(Node& node, Component& component);

    bool setProperty(Node& node, const Property& property, const PropertyValue& value);
    bool setProperty(Component& component, const Property& property, const PropertyValue& value);

    bool setTransform(Node3D& node, const TransformValues& values);

    // Ставит материал в слот slot рендерера одним шагом истории; список
    // слотов дорастает до slot пустыми. false, если ничего не поменялось.
    bool setMaterialSlot(MeshRenderer& renderer, std::size_t slot, ResourceRef material);

    // Материал, отпущенный в окне вида под лучом ray: в слот подмеша, в
    // который попал луч. false - луч мимо мешей.
    bool dropMaterial(const Ray& ray, ResourceRef material);

    // Переносит узел последним ребёнком parent, см. SetParentCommand. false,
    // если так нельзя (canMoveUnder).
    bool setParent(Node& node, Node& parent, KeepWorld keepWorld);

    // Делает камеру главной, меняя её order (см. orderToBecomeMain).
    // false, если камера уже главная или выключена.
    bool makeMainCamera(Camera3D& camera);

private:
    EditorSession& m_session;
};

} // namespace yqlora::editor
