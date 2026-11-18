#include "yqlora/editor/PointPick.hpp"

#include "yqlora/editor/Snapping.hpp"

#include <utility>

namespace yqlora::editor {

void PointPick::request(const void* owner, const Scene& scene)
{
    m_owner = owner;
    m_scene = &scene;
    m_waiting = true;
    m_answer.reset();
}

bool PointPick::waitingFor(const void* owner) const
{
    return m_waiting && m_owner == owner;
}

bool PointPick::waitingIn(const Scene& scene)
{
    if (m_waiting && m_scene != &scene) {
        cancel();
    }

    return m_waiting;
}

void PointPick::cancel()
{
    m_waiting = false;
    m_answer.reset();
}

bool PointPick::answer(const Scene& scene, const Ray& ray, const SnapSettings& snap)
{
    if (!waitingIn(scene)) {
        return false;
    }
    m_answer = dropPoint(scene, ray, snap);
    m_waiting = false;

    return true;
}

std::optional<Vec3> PointPick::take(const void* owner)
{
    if (m_owner != owner || m_waiting) {
        return std::nullopt;
    }
    m_owner = nullptr;

    return std::exchange(m_answer, std::nullopt);
}

void PointPick::showGuide(std::vector<Vec3> line)
{
    m_guide = std::move(line);
}

std::vector<Vec3> PointPick::takeGuide()
{
    return std::exchange(m_guide, {});
}

} // namespace yqlora::editor
