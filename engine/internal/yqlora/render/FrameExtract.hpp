#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"

#include <vector>

namespace yqlora {

class RenderSystem;
class Scene;

// Части Extract (extractFrame) по тому, что они снимают со сцены. Каждая
// читает только своё; extractFrame собирает из них FrameData.

// Виды активных камер в порядке отрисовки (RenderSystem::cameras).
YQLORA_API std::vector<ViewData> extractViews(const RenderSystem& render, float aspect);

// Источники в пределах блока кадра: направленные - первые по порядку,
// точечные и прожекторы - ближайшие к camera. Если пришлось отбросить, об
// этом один раз на сцену пишется в лог.
YQLORA_API std::vector<LightData> extractLights(const RenderSystem& render, const Vec3& camera);

// Включённые MeshRenderer на пространственных узлах с загруженным мешем.
YQLORA_API std::vector<DrawItem> extractDrawItems(const Scene& scene);

// Окружающий свет из настроек освещения проекта.
YQLORA_API EnvironmentData extractEnvironment(const Scene& scene);

} // namespace yqlora
