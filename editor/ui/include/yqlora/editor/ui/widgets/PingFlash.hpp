#pragma once

namespace yqlora::editor {

// Подсветка строки, к которой прислали просьбу показать цель (Pings):
// гаснет за kPingSeconds. Рисуется поверх последнего элемента ImGui;
// startedAt - ImGui::GetTime() в момент просьбы. false - уже погасла.
inline constexpr double kPingSeconds = 1.0;

bool drawPingFlash(double startedAt);

} // namespace yqlora::editor
