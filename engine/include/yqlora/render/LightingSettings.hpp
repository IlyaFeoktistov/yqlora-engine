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

    // Цвет в sRGB, как его выбирает человек.
    Vec3 ambientColor{0.2f, 0.22f, 0.25f};

    float ambientIntensity() const
    {
        return m_ambientIntensity;
    }

    // Не меньше нуля.
    void setAmbientIntensity(float intensity);

    // Линейный цвет, умноженный на интенсивность: то, что идёт в шейдер.
    Vec3 linearAmbient() const;

private:
    float m_ambientIntensity = 1.0f;
};

// Раздел "Освещение" открытого проекта, а без проекта - значения по умолчанию.
YQLORA_API const LightingSettings& lightingSettingsOf(const EngineServices& services);

} // namespace yqlora
