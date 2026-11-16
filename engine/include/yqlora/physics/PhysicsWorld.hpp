#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/physics/PhysicsQuery.hpp"
#include "yqlora/reflection/TypeName.hpp"
#include "yqlora/scene/SceneSystem.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace yqlora {

// Физический мир сцены (Jolt). Шагает в afterFixedUpdate, после
// onFixedUpdate компонентов, по шагу TimeSettings::fixedStep; шагает только
// идущая сцена (копия Play, игра), потому что advance зовут только у неё.
// Сам мир Jolt создаётся с первым телом: сцена без физики его не держит.
// Устройство - docs/architecture/physics.md.
class YQLORA_API PhysicsWorld final : public SceneSystem {
public:
    YQLORA_TYPE(PhysicsWorld, SceneSystem)

    explicit PhysicsWorld(Scene& scene);
    ~PhysicsWorld() override;

    void afterFixedUpdate(float stepSeconds) override;
    // Сглаженные тела показывают положение между двумя последними шагами.
    void update(float deltaSeconds) override;
    void onBranchChanged(Node& node) override;
    // Каналы PhysicsDebug.hpp, кроме запросов: те рисуются в DebugDraw в
    // момент запроса.
    void drawDebug(DebugLines& lines, const DebugChannels& channels) override;
    void addDebugChannels(DebugChannels& channels) const override;

    // Есть ли мир Jolt (появилось ли тело).
    bool simulating() const
    {
        return m_impl != nullptr;
    }

    // Сколько шагов мир сделал с создания.
    std::uint64_t stepCount() const
    {
        return m_steps;
    }

    // Гравитация мира, м/с^2: сначала из настроек "Физика"; setGravity
    // меняет её до конца сцены, изменение настроек ставит снова их значение.
    Vec3 gravity() const;
    void setGravity(const Vec3& gravity);

    // Запросы. Видят тела такими, какими их оставил последний шаг, а в
    // сцене, которая не шагает (редактор), - на месте их узлов; изменённые
    // тела перед запросом перестраиваются. Направление любой длины, путь -
    // maxDistance метров. Без тел - ничего.
    std::optional<RaycastHit> raycast(
        const Vec3& origin,
        const Vec3& direction,
        float maxDistance,
        const QueryFilter& filter = {}
    );
    // Все попадания луча, ближние первыми.
    std::vector<RaycastHit> raycastAll(
        const Vec3& origin,
        const Vec3& direction,
        float maxDistance,
        const QueryFilter& filter = {}
    );

    // Протягивание фигуры: первое, во что она упрётся. Капсула - вдоль своей
    // оси Y, height - вся высота с полусферами, как у CapsuleCollider.
    std::optional<RaycastHit> sphereCast(
        const Vec3& origin,
        float radius,
        const Vec3& direction,
        float maxDistance,
        const QueryFilter& filter = {}
    );
    std::optional<RaycastHit> boxCast(
        const Vec3& center,
        const Vec3& halfExtents,
        const Quat& rotation,
        const Vec3& direction,
        float maxDistance,
        const QueryFilter& filter = {}
    );
    std::optional<RaycastHit> capsuleCast(
        const Vec3& center,
        float radius,
        float height,
        const Quat& rotation,
        const Vec3& direction,
        float maxDistance,
        const QueryFilter& filter = {}
    );

    // Коллайдеры, которые фигура пересекает, по ID узлов и коллайдеров.
    std::vector<ComponentRef<Collider>> overlapSphere(const Vec3& center, float radius, const QueryFilter& filter = {});
    std::vector<ComponentRef<Collider>> overlapBox(
        const Vec3& center,
        const Vec3& halfExtents,
        const Quat& rotation,
        const QueryFilter& filter = {}
    );
    std::vector<ComponentRef<Collider>> overlapCapsule(
        const Vec3& center,
        float radius,
        float height,
        const Quat& rotation,
        const QueryFilter& filter = {}
    );

    // Два коллайдера не сталкиваются (ignore) или снова сталкиваются, что бы
    // ни говорили их слои: снаряд и тот, кто его выпустил. До конца сцены.
    void ignoreCollision(const ComponentRef<Collider>& first, const ComponentRef<Collider>& second, bool ignore = true);
    bool collisionIgnored(const ComponentRef<Collider>& first, const ComponentRef<Collider>& second) const;

private:
    friend struct PhysicsWorldAccess;
    friend struct PhysicsWorldJolt;
    struct Impl;

    // Создаёт мир Jolt, если его ещё нет.
    Impl& impl();
    // Мир, готовый к запросу; nullptr - тел нет.
    Impl* readyForQuery();

    std::unique_ptr<Impl> m_impl;
    std::uint64_t m_steps = 0;
    Vec3 m_gravity;
    // Гравитация настроек, которую мир видел последней: её изменение
    // применяется поверх значения из кода.
    Vec3 m_settingsGravity;
};

} // namespace yqlora
