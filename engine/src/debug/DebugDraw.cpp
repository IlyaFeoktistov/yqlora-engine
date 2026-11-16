#include "yqlora/debug/DebugDraw.hpp"

#include "yqlora/scene/Scene.hpp"

#include <algorithm>

namespace yqlora {

DebugDraw::DebugDraw(Scene& scene)
    : SceneSystem(scene)
{
}

DebugDraw::~DebugDraw() = default;

bool DebugDraw::channelShown(std::string_view channel) const
{
    return m_gameChannels.contains(channel);
}

void DebugDraw::showChannel(std::string_view channel, bool shown)
{
    if (shown) {
        m_gameChannels.emplace(channel);
    } else if (const auto it = m_gameChannels.find(channel); it != m_gameChannels.end()) {
        m_gameChannels.erase(it);
    }
}

DebugChannels DebugDraw::gameChannels() const
{
    return m_enabled ? m_gameChannels : DebugChannels{};
}

bool DebugDraw::collecting(std::string_view channel) const
{
    if (!scene().playing()) {
        return false;
    }

    return m_viewerChannels.contains(channel) || (m_enabled && m_gameChannels.contains(channel));
}

DebugLines* DebugDraw::begin(const DebugOptions& options)
{
    if (!m_used.contains(options.channel)) {
        m_used.emplace(options.channel);
    }
    if (!collecting(options.channel)) {
        return nullptr;
    }
    const GameTime& time = scene().time();
    Entry entry{.channel = std::string(options.channel)};
    if (options.duration > 0.0f) {
        entry.life = Life::Timed;
        entry.expires = time.elapsed() + options.duration;
    } else if (time.inFixedStep()) {
        entry.life = Life::FixedStep;
        entry.stamp = time.fixedSteps();
    } else {
        entry.stamp = time.frame();
    }
    m_entries.push_back(std::move(entry));

    return &m_entries.back().lines;
}

bool DebugDraw::alive(const Entry& entry) const
{
    const GameTime& time = scene().time();
    switch (entry.life) {
    case Life::Frame: return entry.stamp == time.frame();
    case Life::FixedStep: return entry.stamp == time.fixedSteps();
    case Life::Timed: return time.elapsed() < entry.expires;
    }

    return false;
}

void DebugDraw::update(float /*deltaSeconds*/)
{
    std::erase_if(m_entries, [this](const Entry& entry) {
        return !alive(entry);
    });
}

void DebugDraw::drawDebug(DebugLines& lines, const DebugChannels& channels)
{
    for (const Entry& entry : m_entries) {
        if (alive(entry) && channels.contains(entry.channel)) {
            lines.append(entry.lines);
        }
    }
}

void DebugDraw::addDebugChannels(DebugChannels& channels) const
{
    channels.insert(m_used.begin(), m_used.end());
}

void DebugDraw::line(const Vec3& from, const Vec3& to, const DebugOptions& options)
{
    if (DebugLines* lines = begin(options)) {
        lines->line(from, to, debugPen(options));
    }
}

void DebugDraw::ray(const Vec3& origin, const Vec3& direction, const DebugOptions& options)
{
    if (DebugLines* lines = begin(options)) {
        lines->ray(origin, direction, debugPen(options));
    }
}

void DebugDraw::arrow(const Vec3& from, const Vec3& to, const DebugOptions& options)
{
    if (DebugLines* lines = begin(options)) {
        lines->arrow(from, to, debugPen(options));
    }
}

void DebugDraw::point(const Vec3& position, float size, const DebugOptions& options)
{
    if (DebugLines* lines = begin(options)) {
        lines->point(position, size, debugPen(options));
    }
}

void DebugDraw::sphere(const Vec3& center, float radius, const DebugOptions& options)
{
    if (DebugLines* lines = begin(options)) {
        lines->sphere(center, radius, debugPen(options));
    }
}

void DebugDraw::box(const Vec3& center, const Vec3& halfExtents, const Quat& rotation, const DebugOptions& options)
{
    if (DebugLines* lines = begin(options)) {
        lines->box(center, halfExtents, rotation, debugPen(options));
    }
}

void DebugDraw::capsule(
    const Vec3& center,
    float radius,
    float height,
    const Quat& rotation,
    const DebugOptions& options
)
{
    if (DebugLines* lines = begin(options)) {
        lines->capsule(center, radius, height, rotation, debugPen(options));
    }
}

void DebugDraw::axes(const Mat4& transform, float size, const DebugOptions& options)
{
    if (DebugLines* lines = begin(options)) {
        lines->axes(transform, size, options.depthTest);
    }
}

} // namespace yqlora
