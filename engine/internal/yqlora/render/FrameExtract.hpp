#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"

#include <vector>

namespace yqlora {

class FrameMaterials;
class GraphicsSettings;
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

// Настройки теней проекта в их пределах (GraphicsSettings).
YQLORA_API ShadowSettings shadowSettings(const GraphicsSettings& settings);

// Плитки атласа теней: точечным и прожекторам с тенями из frame.lights, от
// ближайших к camera, пока хватает места (точечному - kPointShadowFaces
// плиток). Пишет LightData::firstShadowTile; кому не хватило, остаются без
// теней, об этом один раз на сцену пишется в лог. Без теней в настройках -
// ни плиток, ни теней у источников.
YQLORA_API void extractShadowTiles(const RenderSystem& render, const Vec3& camera, FrameData& frame);

// Половина угла карты теней в радианах: у точечного - грани куба (45
// градусов), у прожектора - внешний угол конуса.
YQLORA_API float shadowTileHalfFov(const LightData& light);

// Дальняя плоскость карты теней точечного и прожектора: его дальность.
YQLORA_API float shadowTileFar(const LightData& light);

// Матрица вида и проекции карты теней прожектора или грани face точечного.
YQLORA_API Mat4 shadowTileMatrix(const LightData& light, int face);

// Окружение сцены (RenderSystem::environment): материал неба и атмосфера.
// Материал неба с шейдером не неба пропускается, о нём один раз пишется в лог.
YQLORA_API EnvironmentData extractEnvironment(const RenderSystem& render, FrameMaterials& materials);

} // namespace yqlora
