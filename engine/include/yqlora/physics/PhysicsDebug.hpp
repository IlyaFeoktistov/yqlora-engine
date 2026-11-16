#pragma once

#include <array>
#include <string_view>

namespace yqlora {

// Каналы отладочной отрисовки физики (DebugDraw, PhysicsWorld::drawDebug).
// Формы, касания и скорости - такие, какими их оставил последний шаг.

// Формы тел: неподвижные, кинематические и динамические разными цветами.
inline constexpr std::string_view kCollidersDebugChannel = "physics.colliders";
// Зоны триггеров.
inline constexpr std::string_view kTriggersDebugChannel = "physics.triggers";
// Точки и нормали касаний после последнего шага.
inline constexpr std::string_view kContactsDebugChannel = "physics.contacts";
// Лучи, протягивания и пересечения запросов: попадание и промах разными цветами.
inline constexpr std::string_view kQueriesDebugChannel = "physics.queries";
// Скорости динамических тел стрелками.
inline constexpr std::string_view kVelocitiesDebugChannel = "physics.velocities";
// Спящие тела своим цветом.
inline constexpr std::string_view kSleepDebugChannel = "physics.sleep";

inline constexpr std::array kPhysicsDebugChannels{
    kCollidersDebugChannel,
    kTriggersDebugChannel,
    kContactsDebugChannel,
    kQueriesDebugChannel,
    kVelocitiesDebugChannel,
    kSleepDebugChannel,
};

} // namespace yqlora
