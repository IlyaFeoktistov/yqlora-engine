#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <cstdint>
#include <memory>

namespace yqlora {
class Scene;
} // namespace yqlora

namespace yqlora::editor {

class EditorSession;

// Чем звенья цепи скреплены друг с другом: точкой (гнётся как угодно) или
// качанием и кручением в пределах (звенья не складываются).
enum class ChainLinkJoint : std::uint8_t {
    Point,
    SwingTwist,
};

// Цепь от start до end: звенья-капсулы радиуса radius шагом около spacing
// (число звеньев - чтобы шаг уложился ровно), массой linkMass каждое.
// Закреплённый конец держится за тело узла под своей точкой, а если его
// нет - за мир; незакреплённый висит свободно.
struct ChainSettings {
    Vec3 start{0.0f, 3.0f, 0.0f};
    Vec3 end{0.0f, 1.0f, 0.0f};
    float spacing = 0.25f;
    float radius = 0.05f;
    float linkMass = 1.0f;
    ChainLinkJoint joint = ChainLinkJoint::SwingTwist;
    bool attachStart = true;
    bool attachEnd = false;
};

// Узлы, за тела которых держатся концы; 0 - за мир.
struct ChainAnchors {
    NodeId start = 0;
    NodeId end = 0;
};

// Самый малый шаг и радиус звена, м.
inline constexpr float kMinChainLinkSize = 0.01f;

// Ветка цепи: узел "Chain" в точке start и звенья в осях мира (родитель -
// без поворота и масштаба), у каждого RigidBody, CapsuleCollider, меш
// капсулы и скрепление с предыдущим звеном. nullptr, если start и end
// совпадают.
std::unique_ptr<Node> buildChain(const ChainSettings& settings, const ChainAnchors& anchors);

// Узел тела, которое касается точки point в сцене (узел с RigidBody над
// коллайдером или сам узел коллайдера); 0 - такого нет.
NodeId bodyNodeAt(Scene& scene, const Vec3& point);

// Ставит цепь в сцену сессии одним шагом истории; концы, которые надо
// закрепить, ищут тела под своими точками. nullptr, если цепь не построить.
Node* createChain(EditorSession& session, const ChainSettings& settings);

} // namespace yqlora::editor

YQLORA_ENUM_NAMES(yqlora::editor::ChainLinkJoint, "Point", "SwingTwist")
