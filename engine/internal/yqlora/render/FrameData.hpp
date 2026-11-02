#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/Light3D.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/Layers.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace yqlora {

struct MeshData;
class ViewPass;

struct CameraData {
    Mat4 view{1.0f};
    Mat4 projection{1.0f};
    Vec3 position{0.0f};
};

// Один вид кадра: камера, чем стереть перед ней и какие слои она рисует.
struct ViewData {
    CameraData camera;
    CameraClear clear = CameraClear::ColorAndDepth;
    // В линейном RGB, как и всё, что рисуется в HDR-цель.
    Vec4 background{0.0f, 0.0f, 0.0f, 1.0f};
    LayerMask mask = kAllLayers;
    // Проходы владельца вида поверх сцены, по порядку. Живут дольше кадра.
    std::vector<ViewPass*> passes;
};

// Один меш в кадре. Данные меша живут в кэше ресурсов до конца кадра.
struct DrawItem {
    const MeshData* mesh = nullptr;
    // По ID и ревизии рендерер узнаёт, что его копия меша в видеопамяти устарела.
    ResourceId meshId = 0;
    std::uint64_t meshRevision = 0;
    Mat4 world{1.0f};
    // Границы меша в мире: по ним Prepare отсекает невидимое.
    Bounds bounds{};
    Vec4 color{1.0f};
    int layer = 0;
    NodeId node = 0; // чей это меш: по нему редактор узнаёт выбранный объект
};

// Источник света в мире, готовый для шейдера.
struct LightData {
    LightKind kind = LightKind::Directional;
    Vec3 position{0.0f};
    // Куда свет идёт, единичный: ось -Z узла.
    Vec3 direction{0.0f, 0.0f, -1.0f};
    // Линейный цвет, умноженный на интенсивность.
    Vec3 color{1.0f};
    // У точечного и прожектора.
    float range = 0.0f;
    // У прожектора: косинусы внутреннего и внешнего угла от оси.
    float cosInner = 1.0f;
    float cosOuter = 1.0f;
};

// Сколько источников шейдер видит за кадр (массивы блока Frame).
inline constexpr std::size_t kMaxDirectionalLights = 4;
inline constexpr std::size_t kMaxPunctualLights = 32;

// Всё, что рендереру нужно знать о кадре. Данные собирает extractFrame,
// рендерер их рисует: виды по очереди, у каждого свои очистка и маска.
// Дерево узлов до бэкенда не доходит.
struct FrameData {
    std::vector<ViewData> views;
    std::vector<DrawItem> drawItems;
    // Не больше kMaxDirectionalLights направленных и kMaxPunctualLights
    // точечных и прожекторов вместе.
    std::vector<LightData> lights;
    // Окружающий свет: линейный цвет, умноженный на интенсивность.
    Vec3 ambient{0.0f};
};

} // namespace yqlora
