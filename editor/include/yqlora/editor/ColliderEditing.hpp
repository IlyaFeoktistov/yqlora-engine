#pragma once

#include "yqlora/editor/ShapeHandles.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/render/MeshData.hpp"

#include <optional>
#include <vector>

namespace yqlora {
class Collider;
class Node;
class Node3D;
class ResourceCache;
} // namespace yqlora

namespace yqlora::editor {

// Фигура коллайдера для ручек: коробка, сфера, капсула и цилиндр.
// nullopt у коллайдеров-мешей: их форму задаёт меш, а не размеры.
std::optional<HandleShape> colliderHandleShape(const Collider& collider);

// Ставит коллайдеру размеры и центр фигуры shape (поворот - его свойство,
// ручки его не меняют). Коллайдеры без фигуры для ручек не меняются.
void applyHandleShape(Collider& collider, const HandleShape& shape);

// Где фигура коллайдера в мире: узел, затем центр и поворот коллайдера.
Mat4 colliderWorldMatrix(const Collider& collider, const Node3D& node);

// Границы мешей узла и его потомков в осях node. В ветки со своим
// RigidBody не заходит: это другие тела. nullopt - мешей нет.
std::optional<Bounds> branchMeshBounds(const Node3D& node, ResourceCache& resources);

// Подгоняет коллайдер узла node под его меши: коробка - их размер и
// центр, сфера - наибольшая половина размера, капсула и цилиндр - высота
// вдоль Y и радиус поперёк; поворот сбрасывается, потому что границы - в
// осях узла. Коллайдер-меш берёт меш рендерера узла. Коллайдер может быть
// ещё не в сцене: узел задан отдельно. false - подгонять не подо что.
bool fitCollider(Collider& collider, const Node3D& node, ResourceCache& resources);

// Есть ли подо что подогнать коллайдер узла node (см. fitCollider).
bool canFitCollider(const Collider& collider, const Node3D& node, ResourceCache& resources);

// Коллайдеры тела, к которому относится узел: от ближайшего узла с
// RigidBody (сам узел или выше; без него - сам узел) по его ветке, не
// заходя в ветки со своим RigidBody.
std::vector<const Collider*> bodyColliders(const Node& node);

} // namespace yqlora::editor
