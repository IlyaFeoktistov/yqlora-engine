#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/project/SettingsSection.hpp"

namespace yqlora {

class EngineServices;

// Раздел "Освещение" настроек проекта: окружающий свет, пока у сцены нет
// своих настроек окружения. Это постоянное рассеянное слагаемое во всех
// направлениях, а не освещение по окружению.
class YQLORA_API LightingSettings final : public SettingsSection {
public:
    YQLORA_TYPE(LightingSettings, SettingsSection)

    // Цвет в sRGB, как его выбирает человек; голубоватый, как от неба.
    Vec3 ambientColor{0.5f, 0.6f, 0.75f};

    float ambientIntensity() const
    {
        return m_ambientIntensity;
    }

    // Яркость белого в нитах. Не меньше нуля.
    void setAmbientIntensity(float intensity);

    // Яркость окружающего света в нитах: линейный цвет, умноженный на
    // интенсивность; то, что идёт в шейдер.
    Vec3 linearAmbient() const;

private:
    // Небо в пасмурный день.
    float m_ambientIntensity = 10000.0f;
};

// Раздел "Освещение" открытого проекта, а без проекта - значения по умолчанию.
YQLORA_API const LightingSettings& lightingSettingsOf(const EngineServices& services);

} // namespace yqlora
