#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/render/ToneMapping.hpp"
#include "yqlora/scene/Layers.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <cstdint>

namespace yqlora {

// Что камера стирает перед тем, как рисовать. Вторая камера поверх первой
// (руки игрока поверх мира) стирает только глубину: мир остаётся, а руки
// не уходят в стены.
enum class CameraClear : std::uint8_t {
    ColorAndDepth,
    DepthOnly,
    Nothing,
};

// EV100 новой камеры, солнечный день: белое под солнцем в 100000 лк выходит
// около 0.8. Вне класса: члены экспортируемого класса в dll не видны как
// constexpr-значения тем, кто берёт на них ссылку.
inline constexpr float kDefaultCameraExposure = 15.0f;
inline constexpr ToneMapping kDefaultCameraToneMapping = ToneMapping::AgX;

// Перспективная камера. Смотрит вдоль своей оси -Z.
// Соотношение сторон это свойство области вывода, а не камеры,
// поэтому передаётся при построении проекции. Камеры сцены рисуются по
// очереди (см. RenderSystem), первая из них - главная.
class YQLORA_API Camera3D : public Node3D {
public:
    explicit Camera3D(std::string name = "Camera3D");

    YQLORA_TYPE(Camera3D, Node3D)

    // вертикальный угол обзора
    float fovDegrees() const
    {
        return m_fovDegrees;
    }

    float nearPlane() const
    {
        return m_near;
    }

    float farPlane() const
    {
        return m_far;
    }

    void setFovDegrees(float fov);
    void setClipPlanes(float nearPlane, float farPlane);

    // По одной плоскости, для инспектора и загрузки сцены. Дальняя плоскость
    // всегда остаётся дальше ближней: при конфликте сдвигается дальняя.
    void setNearPlane(float nearPlane);
    void setFarPlane(float farPlane);

    Mat4 viewMatrix() const;
    Mat4 projectionMatrix(float aspect) const;

    // Порядок отрисовки: меньше - раньше. Первая камера сцены - главная.
    int order() const
    {
        return m_order;
    }

    void setOrder(int order)
    {
        m_order = order;
    }

    // Слои узлов, которые камера рисует.
    LayerMask cullingMask() const
    {
        return m_cullingMask;
    }

    void setCullingMask(LayerMask mask)
    {
        m_cullingMask = mask;
    }

    CameraClear clear() const
    {
        return m_clear;
    }

    void setClear(CameraClear clear)
    {
        m_clear = clear;
    }

    // Цвет, которым камера стирает кадр при CameraClear::ColorAndDepth.
    const Vec4& background() const
    {
        return m_background;
    }

    void setBackground(const Vec4& color)
    {
        m_background = color;
    }

    // Экспозиция в EV100: 15 - солнечный день, около 7 - комната. Больше -
    // темнее кадр (см. exposureFactor).
    float exposure() const
    {
        return m_exposure;
    }

    void setExposure(float ev100)
    {
        m_exposure = ev100;
    }

    ToneMapping toneMapping() const
    {
        return m_toneMapping;
    }

    void setToneMapping(ToneMapping toneMapping)
    {
        m_toneMapping = toneMapping;
    }

protected:
    void onEnterScene() override;
    void onExitScene() override;

private:
    float m_fovDegrees = 60.0f;
    float m_near = 0.1f;
    float m_far = 1000.0f;
    int m_order = 0;
    LayerMask m_cullingMask = kAllLayers;
    CameraClear m_clear = CameraClear::ColorAndDepth;
    Vec4 m_background{0.08f, 0.09f, 0.12f, 1.0f};
    float m_exposure = kDefaultCameraExposure;
    ToneMapping m_toneMapping = kDefaultCameraToneMapping;
};

// Множитель яркости при экспозиции ev100: 1 / (1.2 * 2^ev100), как у
// фотоаппарата с ISO 100 (1.2 - запас до насыщения датчика, 78 / 65).
// Яркость в нитах, умноженная на него, около 1 у белого при правильной
// экспозиции.
YQLORA_API float exposureFactor(float ev100);

// Имя свойства order в реестре: по нему редактор меняет порядок командой с отменой.
inline constexpr char kCameraOrderProperty[] = "order";

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::CameraClear, "ColorAndDepth", "DepthOnly", "Nothing")
