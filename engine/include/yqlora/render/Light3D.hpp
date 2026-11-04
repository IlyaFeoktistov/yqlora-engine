#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <cstdint>

namespace yqlora {

enum class LightKind : std::uint8_t {
    Directional,
    Point,
    Spot,
};

// Тени источника: нет, резкие (одно сравнение глубины) или мягкие - полутень
// по размеру источника и расстоянию от заслоняющего до поверхности.
enum class ShadowMode : std::uint8_t {
    None,
    Hard,
    Soft,
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

    ShadowMode shadows() const
    {
        return m_shadows;
    }

    void setShadows(ShadowMode shadows)
    {
        m_shadows = shadows;
    }

    // Насколько тень гасит прямой свет источника: 1 - целиком, 0 - тени нет.
    float shadowStrength() const
    {
        return m_shadowStrength;
    }

    void setShadowStrength(float strength);

    // Множитель ширины полутени у Soft: 1 - по размеру источника, 0 - резко,
    // больше - мягче, чем физически. Не меньше нуля.
    float shadowSoftness() const
    {
        return m_shadowSoftness;
    }

    void setShadowSoftness(float softness);

    // Сдвиг точки к источнику и вдоль нормали перед сравнением с картой
    // теней, в текселях карты (так он не зависит от её размера и каскада):
    // против полос самозатенения ("прыщей") и отрыва тени от объекта. Не
    // меньше нуля.
    float shadowBias() const
    {
        return m_shadowBias;
    }

    void setShadowBias(float bias);

    float shadowNormalBias() const
    {
        return m_shadowNormalBias;
    }

    void setShadowNormalBias(float bias);

protected:
    Light3D(std::string name, LightKind kind, float intensity, ShadowMode shadows);

    void onEnterScene() override;
    void onExitScene() override;

private:
    LightKind m_kind;
    Vec3 m_color{1.0f};
    float m_intensity = 1.0f;
    ShadowMode m_shadows = ShadowMode::None;
    float m_shadowStrength = 1.0f;
    float m_shadowSoftness = 1.0f;
    float m_shadowBias = 0.5f;
    float m_shadowNormalBias = 1.0f;
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::ShadowMode, "None", "Hard", "Soft")
