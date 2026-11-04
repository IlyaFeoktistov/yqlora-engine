#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"

#include <span>
#include <vector>

namespace yqlora {

// Каскад тени солнца: ортографическая карта над частью пирамиды камеры.
struct ShadowCascade {
    Mat4 viewProjection{1.0f};
    // Дальняя граница каскада: глубина вида в метрах вдоль взгляда камеры.
    float splitFar = 0.0f;
    // Ширина квадрата карты и глубина её коробки в метрах.
    float width = 0.0f;
    float depthRange = 0.0f;
};

// Дальние границы count каскадов от near до distance: смесь логарифмической
// (вблизи камеры каскады мельче) и равномерной схемы. Последняя - distance.
YQLORA_API std::vector<float> cascadeSplits(float near, float distance, int count);

// Каскады солнца для камеры: каждый - шар вокруг своего куска пирамиды
// (размер не меняется при повороте камеры), центр привязан к текселям карты
// стороной mapSize (тень не дрожит, когда камера движется), коробка к
// солнцу вытянута до отбрасывающих тень из items, чтобы тень от них не
// обрезалась. toLight - единичное направление к солнцу.
YQLORA_API std::vector<ShadowCascade> fitShadowCascades(
    const CameraData& camera,
    const Vec3& toLight,
    const ShadowSettings& settings,
    std::span<const DrawItem> items
);

} // namespace yqlora
