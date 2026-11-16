#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Event.hpp"

#include <cstdint>

namespace yqlora {

// Как move понимает скорость на склоне, по которому идёт персонаж.
// KeepHorizontal - скорость по плану (вид сверху) остаётся той, что
// просили, вверх и вниз, как идёт человек; AlongSurface - скорость
// меряется вдоль склона, поэтому по плану на склоне медленнее.
enum class SlopeSpeed : std::uint8_t {
    KeepHorizontal,
    AlongSurface,
};

// Пределы свойств персонажа.
inline constexpr float kMinCharacterSize = 0.01f;
inline constexpr float kMaxCharacterSlope = 89.0f;

// Персонаж, которого игра водит по миру: капсула, которая сталкивается,
// скользит вдоль стен, берёт ступени, держится земли на спуске,
// останавливается у крутых склонов, едет на платформах и толкает тела.
// Сам он не падает и не разгоняется: скорость, гравитацию и прыжок задаёт
// игра в move. Узел без RigidBody: тело и персонаж двигали бы его оба.
// Другие тела, запросы и триггеры видят персонажа как кинематическое тело
// его узла. Устройство - docs/architecture/physics.md.
class YQLORA_API CharacterController : public Component {
public:
    YQLORA_TYPE(CharacterController, Component)

    // Капсула в метрах: радиус, вся высота с полусферами и центр от узла.
    // Капсула всегда стоит вертикально и не поворачивается с узлом;
    // центр - в осях мира, по умолчанию узел - у ног.
    float radius() const
    {
        return m_radius;
    }

    void setRadius(float radius);

    float height() const
    {
        return m_height;
    }

    void setHeight(float height);

    const Vec3& center() const
    {
        return m_center;
    }

    void setCenter(const Vec3& center);

    // Земля круче этого угла от горизонта, градусы, - стена: в неё
    // упираются и по ней съезжают.
    float slopeLimit() const
    {
        return m_slopeLimit;
    }

    void setSlopeLimit(float degrees);

    // Ступень до этой высоты, м, берётся без прыжка и без замедления, если
    // включён walkStairs.
    float stepHeight() const
    {
        return m_stepHeight;
    }

    void setStepHeight(float height);

    bool walkStairs() const
    {
        return m_walkStairs;
    }

    void setWalkStairs(bool walk);

    // На спуске по лестнице или склону ноги остаются на земле, если она
    // ниже не больше чем на stickDistance, м; глубже - это обрыв, падение.
    bool stickToGround() const
    {
        return m_stickToGround;
    }

    void setStickToGround(bool stick);

    float stickDistance() const
    {
        return m_stickDistance;
    }

    void setStickDistance(float distance);

    SlopeSpeed slopeSpeed() const
    {
        return m_slopeSpeed;
    }

    void setSlopeSpeed(SlopeSpeed speed)
    {
        m_slopeSpeed = speed;
    }

    // Зазор, м, на котором капсула держится от стен: меньше застреваний.
    float skinWidth() const
    {
        return m_skinWidth;
    }

    void setSkinWidth(float width);

    // Масса, кг (как сильно его толкают тела), и сила, Н, с которой он
    // толкает динамические тела.
    float mass() const
    {
        return m_mass;
    }

    void setMass(float mass);

    float pushForce() const
    {
        return m_pushForce;
    }

    void setPushForce(float force);

    // Двигает персонажа со скоростью velocity, м/с, в течение deltaSeconds
    // и ставит узел туда, куда он дошёл. На земле скорость вниз не берётся
    // (вниз по склону и ступеням ведёт stickToGround), скорость по земле
    // ложится на склон по slopeSpeed, а скорость платформы под ногами
    // прибавляется. Вне сцены или выключенный - ничего.
    void move(const Vec3& velocity, float deltaSeconds);

    // Стоит на земле, по которой можно ходить (не круче slopeLimit).
    bool isGrounded() const;
    // Нормаль и скорость земли под ним; без земли - ноль.
    Vec3 groundNormal() const;
    Vec3 groundVelocity() const;
    // С какой скоростью он прошёл последний move, м/с.
    Vec3 velocity() const;

    // Меняет высоту капсулы (приседание) и отказывает, если для новой нет
    // места: форма проверяется до того, как принята. Узел не двигается.
    bool resize(float height);

    // Коснулся в move узла другого тела: на каждое новое касание по разу.
    Event<NodeRef> onControllerHit;

protected:
    void onEnterScene() override;
    void onExitScene() override;

private:
    // Капсула или настройки изменились: персонаж перестроится перед
    // следующим движением или шагом.
    void changed();

    float m_radius = 0.4f;
    float m_height = 1.8f;
    Vec3 m_center{0.0f, 0.9f, 0.0f};
    float m_slopeLimit = 45.0f;
    float m_stepHeight = 0.3f;
    bool m_walkStairs = true;
    bool m_stickToGround = true;
    float m_stickDistance = 0.5f;
    SlopeSpeed m_slopeSpeed = SlopeSpeed::KeepHorizontal;
    float m_skinWidth = 0.02f;
    float m_mass = 70.0f;
    float m_pushForce = 100.0f;
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::SlopeSpeed, "KeepHorizontal", "AlongSurface")
