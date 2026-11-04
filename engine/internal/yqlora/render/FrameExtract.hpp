#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"

#include <vector>

namespace yqlora {

class FrameMaterials;
class RenderSystem;
class Scene;

// Части Extract (extractFrame) по тому, что они снимают со сцены. Каждая
// читает только своё; extractFrame собирает из них FrameData.

// Виды активных камер в порядке отрисовки (RenderSystem::cameras).
YQLORA_API std::vector<ViewData> extractViews(const RenderSystem& render, float aspect);

// Источники в пределах блока кадра: направленные - первые по порядку, а
// солнце (LightData::sun) - первым из них; точечные и прожекторы -
// ближайшие к camera. Если пришлось отбросить, об этом один раз на сцену
// пишется в лог.
YQLORA_API std::vector<LightData> extractLights(const RenderSystem& render, const Vec3& camera);

// Включённые MeshRenderer на пространственных узлах с загруженным мешем: по
// DrawItem на подмеш, материалы слотов и их замены - в frame.
YQLORA_API void extractDrawItems(const Scene& scene, FrameMaterials& materials, FrameData& frame);

// Окружение сцены (RenderSystem::environment): материал неба и атмосфера.
// Материал неба с шейдером не неба пропускается, о нём один раз пишется в лог.
YQLORA_API EnvironmentData extractEnvironment(const RenderSystem& render, FrameMaterials& materials);

} // namespace yqlora
