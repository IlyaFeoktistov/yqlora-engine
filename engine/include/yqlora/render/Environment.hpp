#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/render/Atmosphere.hpp"
#include "yqlora/resource/BuiltinResources.hpp"
#include "yqlora/scene/Node.hpp"

#include <cstdint>

namespace yqlora {

// Откуда окружающий свет: из неба, из трёх цветов по высоте или один цвет.
enum class AmbientSource : std::uint8_t {
    Sky,
    Gradient,
    Color,
};

// Небо, окружающий свет и атмосфера сцены. Работает первое активное
// окружение сцены (RenderSystem::environment); без него сцена без неба,
// окружающего света и атмосферы. Не пространственный узел: положение у
// окружения нет.
class YQLORA_API Environment final : public Node {
public:
    explicit Environment(std::string name = "Environment");

    YQLORA_TYPE(Environment, Node)

    // Материал с шейдером неба; пустая ссылка - неба нет.
    ResourceRef sky{builtin::kDefaultSkyMaterial};

    AmbientSource ambientSource = AmbientSource::Sky;

    // Множитель окружающего света: 1 - физически. Не меньше нуля.
    float ambientIntensity() const
    {
        return m_ambientIntensity;
    }

    void setAmbientIntensity(float intensity);

    // Для AmbientSource::Gradient: цвета (sRGB) зенита, горизонта и земли.
    Vec3 skyColor{0.35f, 0.55f, 0.85f};
    Vec3 equatorColor{0.7f, 0.75f, 0.8f};
    Vec3 groundColor{0.3f, 0.27f, 0.22f};
    // Для AmbientSource::Color: один цвет (sRGB).
    Vec3 ambientColor{0.6f, 0.7f, 0.85f};

    // Яркость градиента и цвета в нитах для белого. Не меньше нуля.
    float gradientLuminance() const
    {
        return m_gradientLuminance;
    }

    void setGradientLuminance(float luminance);

    Atmosphere atmosphere{};

protected:
    void onEnterScene() override;
    void onExitScene() override;

private:
    float m_ambientIntensity = 1.0f;
    // Небо в пасмурный день.
    float m_gradientLuminance = 10000.0f;
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::AmbientSource, "Sky", "Gradient", "Color")
