#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/scene/Node.hpp"

#include <memory>
#include <string>

namespace yqlora::editor {

class EditorSession;

// Цепь тел (PhysicsChain): узел name в точке position мира, звенья от него
// назад вдоль +Z.
struct BodyChainSettings {
    std::string name = "BodyChain";
    Vec3 position{0.0f};
    int segments = 8;
    float length = 2.0f;
    float radius = 0.1f;
    float mass = 4.0f;
};

// Ветка цепи тел: узел с PhysicsChain и по дочернему узлу на звено с мешем
// капсулы, который лежит вдоль звена и следует за ним.
std::unique_ptr<Node> buildBodyChain(const BodyChainSettings& settings);

// Ставит цепь тел в сцену сессии одним шагом истории.
Node* createBodyChain(EditorSession& session, const BodyChainSettings& settings);

} // namespace yqlora::editor
