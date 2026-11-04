#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <cstdint>

namespace yqlora {

enum class LightKind : std::uint8_t {
    Directional,
    Point,
    Spot,
};

// Источник света сцены, основа DirectionalLight3D, PointLight3D и
// SpotLight3D; сам по себе не создаётся. Светит вдоль своей оси -Z, как
// камера смотрит. О материалах ничего не знает: они встречаются только в
// шейдере.
class YQLORA_API Light3D : public Node3D {
public:
    YQLORA_TYPE(Light3D, Node3D)

    LightKind kind() const
    {
        return m_kind;
    }

    // Цвет в sRGB, как его выбирает человек; свет считается в линейном.
    const Vec3& color() const
    {
        return m_color;
    }

    void setColor(const Vec3& color)
    {
        m_color = color;
    }

    // В физических единицах, как в glTF: у направленного - освещённость в
    // люксах, у точечного и прожектора - сила света в канделах. Белая
    // рассеивающая поверхность, повёрнутая к E люксам, имеет яркость E / пи
    // нит. Не меньше нуля.
    float intensity() const
    {
        return m_intensity;
    }

    void setIntensity(float intensity);

protected:
    Light3D(std::string name, LightKind kind, float intensity);

    void onEnterScene() override;
    void onExitScene() override;

private:
    LightKind m_kind;
    Vec3 m_color{1.0f};
    float m_intensity = 1.0f;
};

} // namespace yqlora
