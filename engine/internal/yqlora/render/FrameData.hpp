#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/Layers.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <cstdint>
#include <vector>

namespace yqlora {

struct MeshData;

struct CameraData {
    Mat4 view{1.0f};
    Mat4 projection{1.0f};
    Vec3 position{0.0f};
};

// Один вид кадра: камера, чем стереть перед ней и какие слои она рисует.
struct ViewData {
    CameraData camera;
    CameraClear clear = CameraClear::ColorAndDepth;
    Vec4 background{0.0f, 0.0f, 0.0f, 1.0f};
    LayerMask mask = kAllLayers;
};

// Один меш в кадре. Данные меша живут в кэше ресурсов до конца кадра.
struct DrawItem {
    const MeshData* mesh = nullptr;
    // По ID и ревизии бэкенд узнаёт, что его копия меша в видеопамяти устарела.
    ResourceId meshId = 0;
    std::uint64_t meshRevision = 0;
    Mat4 world{1.0f};
    Vec4 color{1.0f};
    int layer = 0;
    NodeId node = 0; // чей это меш: по нему рендерер узнаёт выбранный объект
};

// Вспомогательная графика редактора, которой нет в игре. Рисуется поверх
// последнего вида кадра.
struct EditorOverlay {
    bool grid = false;
    // Выбранные узлы обводятся контуром.
    std::vector<NodeId> selected;
    Vec4 outlineColor{1.0f, 0.6f, 0.1f, 1.0f};
};

// Всё, что рендереру нужно знать о кадре. Данные собирает extractFrame,
// рендерер их рисует: виды по очереди, у каждого свои очистка и маска.
// Дерево узлов до бэкенда не доходит.
struct FrameData {
    std::vector<ViewData> views;
    std::vector<DrawItem> drawItems;
    EditorOverlay editor;
};

} // namespace yqlora
