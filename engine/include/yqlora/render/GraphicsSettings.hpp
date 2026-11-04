#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/project/SettingsSection.hpp"
#include "yqlora/render/GraphicsApi.hpp"

namespace yqlora {

class EngineServices;

// Раздел Graphics настроек проекта. Применяется сразу: смена API пересоздаёт
// только рендер (окно, если API нужно другое, рендерер, отрисовку
// интерфейса), а не всё приложение; vsync переключается без пересоздания.
class YQLORA_API GraphicsSettings final : public SettingsSection {
public:
    YQLORA_TYPE(GraphicsSettings, SettingsSection)

    GraphicsApi windowsApi = GraphicsApi::OpenGL;
    GraphicsApi linuxApi = GraphicsApi::OpenGL;
    // Не рисовать кадры быстрее, чем обновляется экран.
    bool vsync = true;

    // Тени вообще: выключено - ни карт теней, ни их проходов, у всех
    // источников как ShadowMode::None.
    bool shadows = true;
    // Дальше этого расстояния от камеры в метрах солнце теней не даёт.
    float shadowDistance = 100.0f;
    // Каскадов у солнца (1..kMaxShadowCascades): больше - резче вблизи и
    // дальше, но дороже.
    int shadowCascades = 4;
    // Сторона карты одного каскада и всего атласа теней точечных и
    // прожекторов в текселях.
    int shadowCascadeSize = 2048;
    int shadowAtlasSize = 4096;
    // Отсчёты поиска заслоняющих и фильтра мягкой тени: больше - глаже
    // полутень, но дороже.
    int shadowBlockerSamples = 16;
    int shadowFilterSamples = 32;
    // Предел ширины полутени в текселях карты: и ограничение мягкости, и
    // область поиска заслоняющих.
    float shadowMaxPenumbra = 32.0f;

    // API для системы, на которой работает программа.
    GraphicsApi currentPlatformApi() const;
};

// Раздел Graphics открытого проекта, а без проекта - значения по умолчанию.
YQLORA_API const GraphicsSettings& graphicsSettingsOf(const EngineServices& services);

} // namespace yqlora
