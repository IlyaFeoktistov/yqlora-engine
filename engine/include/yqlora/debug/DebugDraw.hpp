#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/debug/DebugChannels.hpp"
#include "yqlora/debug/DebugLines.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/TypeName.hpp"
#include "yqlora/scene/SceneSystem.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

// Как рисуется примитив DebugDraw.
struct DebugOptions {
    // В sRGB.
    Vec4 color{1.0f};
    // 0 - один кадр: до следующего кадра, а нарисованное в фиксированном
    // шаге - до следующего шага, поэтому не мигает, когда в кадре шага нет.
    // Иначе - секунды игрового времени.
    float duration = 0.0f;
    // Выключен - видно сквозь стены.
    bool depthTest = true;
    std::string_view channel = kDefaultDebugChannel;
};

// Отладочная отрисовка сцены: линии и фигуры поверх мира из кода игры и
// подсистем (лучи запросов, векторы, зоны). Примитив принадлежит каналу и
// собирается, только если канал кто-то показывает: окно редактора (каналы
// зрителей) или сама игра (enabled и её каналы), поэтому выключенная
// отладка почти ничего не стоит. Собирает только идущая сцена. Устройство -
// docs/architecture/rendering.md.
//
//   debugDraw().ray(origin, direction * 10.0f, {.color = kRed, .duration = 2.0f});
class YQLORA_API DebugDraw final : public SceneSystem {
public:
    YQLORA_TYPE(DebugDraw, SceneSystem)

    explicit DebugDraw(Scene& scene);
    ~DebugDraw() override;

    void line(const Vec3& from, const Vec3& to, const DebugOptions& options = {});
    void ray(const Vec3& origin, const Vec3& direction, const DebugOptions& options = {});
    void arrow(const Vec3& from, const Vec3& to, const DebugOptions& options = {});
    void point(const Vec3& position, float size, const DebugOptions& options = {});
    void sphere(const Vec3& center, float radius, const DebugOptions& options = {});
    void box(const Vec3& center, const Vec3& halfExtents, const Quat& rotation, const DebugOptions& options = {});
    void capsule(
        const Vec3& center,
        float radius,
        float height,
        const Quat& rotation,
        const DebugOptions& options = {}
    );
    // Оси своих цветов: цвет options не берётся.
    void axes(const Mat4& transform, float size, const DebugOptions& options = {});

    // Отладка самой игры: в собранной игре видно только это. Выключена,
    // пока её не включит код, например по клавише в отладочной сборке.
    bool enabled() const
    {
        return m_enabled;
    }

    void setEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

    // Каналы, которые показывает игра; сначала - только канал по умолчанию.
    bool channelShown(std::string_view channel) const;
    void showChannel(std::string_view channel, bool shown = true);

    // Что показывает игра сейчас: её каналы, если отладка включена, иначе ничего.
    DebugChannels gameChannels() const;

    // Каналы, которые показывают окна редактора, вместе: их задаёт
    // редактор каждый кадр.
    void setViewerChannels(DebugChannels channels)
    {
        m_viewerChannels = std::move(channels);
    }

    // Собирается ли канал: сцена идёт и его кто-то показывает. Подсистемам -
    // чтобы не готовить примитивы зря.
    bool collecting(std::string_view channel) const;

    // Убирает примитивы, срок которых вышел.
    void update(float deltaSeconds) override;

    void drawDebug(DebugLines& lines, const DebugChannels& channels) override;
    // Каналы, в которые рисовали: в меню выбора они есть, даже пока не собираются.
    void addDebugChannels(DebugChannels& channels) const override;

private:
    enum class Life {
        Frame,
        FixedStep,
        Timed,
    };

    struct Entry {
        std::string channel;
        Life life = Life::Frame;
        // Кадр или шаг, в котором нарисован (GameTime::frame, fixedSteps).
        std::uint64_t stamp = 0;
        // Для Timed: до какого GameTime::elapsed виден.
        double expires = 0.0;
        DebugLines lines;
    };

    // Линии нового примитива; nullptr - канал никто не показывает.
    DebugLines* begin(const DebugOptions& options);
    bool alive(const Entry& entry) const;

    std::vector<Entry> m_entries;
    DebugChannels m_used;
    DebugChannels m_gameChannels{std::string(kDefaultDebugChannel)};
    DebugChannels m_viewerChannels;
    bool m_enabled = false;
};

// Перо примитива по его настройкам.
inline DebugPen debugPen(const DebugOptions& options)
{
    return {.color = options.color, .depthTest = options.depthTest};
}

} // namespace yqlora
