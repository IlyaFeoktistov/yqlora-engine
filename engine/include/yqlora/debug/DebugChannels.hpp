#pragma once

#include <functional>
#include <set>
#include <string>
#include <string_view>

namespace yqlora {

// Каналы отладочной отрисовки по именам: что показывать или что система
// умеет рисовать. Свои каналы игры - любые имена, каналы подсистем - с
// приставкой подсистемы ("physics.contacts").
using DebugChannels = std::set<std::string, std::less<>>;

// Канал примитивов, которым канал не задан.
inline constexpr std::string_view kDefaultDebugChannel = "game";

} // namespace yqlora
