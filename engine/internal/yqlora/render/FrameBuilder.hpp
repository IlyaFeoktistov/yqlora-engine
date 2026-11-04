#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"

namespace yqlora {

class Camera3D;
class Scene;

// Extract: снимок сцены для рендерера на текущий кадр. Виды всех активных
// камер в порядке отрисовки (см. RenderSystem) и всё, что рисовать. После
// него рендерер сцену не читает.
YQLORA_API FrameData extractFrame(const Scene& scene, float aspect);

// Вид через одну камеру, например для превью камеры в редакторе.
YQLORA_API ViewData cameraView(const Camera3D& camera, float aspect);

// Экспозиция и тональная компрессия камеры: их берёт и вид не через
// камеру сцены (окно сцены редактора показывает кадр как главная камера).
// nullptr - как у новой камеры.
YQLORA_API ViewExposure viewExposure(const Camera3D* camera);

} // namespace yqlora
