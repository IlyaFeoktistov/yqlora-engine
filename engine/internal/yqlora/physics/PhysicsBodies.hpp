#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/physics/BodyPose.hpp"
#include "yqlora/physics/JoltShapes.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/NodeId.hpp"
#include "yqlora/scene/ObjectId.hpp"

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/MotionType.h>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace JPH {

class BodyInterface;

} // namespace JPH

namespace yqlora {

class Component;
class MeshShapeCache;
class PhysicsMaterialCache;
class Node;
class ResourceCache;
class Scene;

// Коллайдер тела: компонент и его узел.
struct BodyPart {
    ComponentId collider = 0;
    NodeId node = 0;
};

// Чья сторона касания: узел-хозяин тела и коллайдер; trigger - датчик
// коллайдера-триггера.
struct ContactSide {
    NodeId owner = 0;
    BodyPart part;
    bool trigger = false;
};

// Тела Jolt вслед за деревом сцены. Хозяин тела - узел: с RigidBody (тело
// из коллайдеров узла и потомков до следующего RigidBody) или с
// коллайдерами без RigidBody над ними (неявное неподвижное тело из
// коллайдеров этого узла). Изменения отмечаются сразу, а тела
// перестраиваются в flush, перед шагом. Устройство - docs/architecture/physics.md.
class PhysicsBodies {
public:
    PhysicsBodies(
        Scene& scene,
        JPH::BodyInterface& bodies,
        MeshShapeCache& meshShapes,
        PhysicsMaterialCache& materials
    );
    ~PhysicsBodies();

    PhysicsBodies(const PhysicsBodies&) = delete;
    PhysicsBodies& operator=(const PhysicsBodies&) = delete;

    // В ветке node мог смениться хозяин коллайдеров или состав тел.
    void branchChanged(const Node& node);
    // Коллайдер или RigidBody уходит из сцены: его тело перестроится.
    void componentLeft(const Component& component);

    // Перестраивает изменённые тела и те, у которых дочерний коллайдер
    // сдвинулся относительно узла тела, сменился масштаб узла тела или
    // перевыгружен меш коллайдера; сменился материал по умолчанию в
    // настройках - все тела.
    void flush();

    // Перед шагом: узлы, сдвинутые кодом или редактором, переносят свои тела
    // (кинематические - целью шага stepSeconds).
    void beforeStep(float stepSeconds);
    // После шага: динамические тела пишут положение в узлы (без сглаживания).
    void afterStep();
    // Сглаженные динамические тела пишут в узлы положение между двумя
    // последними шагами; fraction - доля шага, см. GameTime::fixedStepFraction.
    void interpolate(float fraction);

    std::size_t bodyCount() const
    {
        return m_bodies.size();
    }

    // Сторона касания тела Jolt joltId (BodyID::GetIndexAndSequenceNumber)
    // по номеру коллайдера part (bodyPartOf); nullopt - тела уже нет.
    std::optional<ContactSide> sideOf(std::uint32_t joltId, std::uint32_t part) const;

    // Тело коллайдера есть и не движется (спит или неподвижно): Jolt о его
    // касаниях молчит, но они продолжаются.
    bool resting(ComponentId collider) const;

    // Будит все подвижные тела: сменились значения их материалов.
    void wakeAll();

    // Тело узла-хозяина; невалидный ID, если у узла тела нет.
    JPH::BodyID bodyOf(NodeId owner) const;

private:
    // Датчик коллайдера-триггера: своё кинематическое тело на месте
    // коллайдера, offset - от положения тела хозяина.
    struct Sensor {
        JPH::BodyID id;
        BodyPart part;
        BodyPose offset;
    };

    struct Body {
        // Невалидный - у хозяина только триггеры.
        JPH::BodyID id;
        JPH::EMotionType motion = JPH::EMotionType::Static;
        bool interpolation = false;
        // Глубина узла в дереве: родительские тела пишутся раньше дочерних.
        int depth = 0;
        // Положение, которое тело последним записало в узел или взяло у узла:
        // отличие от него - узел сдвинули кодом или редактором.
        BodyPose remembered;
        // Два последних шага - для сглаживания.
        BodyPose previous;
        BodyPose current;
        // Сглаженное положение, записанное последним: то же - писать нечего.
        BodyPose lastTarget;
        // Ревизии трансформов между дочерними коллайдерами и узлом тела.
        std::vector<std::uint64_t> childRevisions;
        // Масштаб осей узла тела: он в размерах фигур.
        Vec3 scale{1.0f};
        // Меши коллайдеров и ревизии, из которых готовились их формы.
        std::vector<std::pair<ResourceId, std::uint64_t>> meshRevisions;
        // Коллайдеры формы по номерам частей (PlacedShape::part).
        std::vector<BodyPart> parts{};
        // Номер коллайдера, если форма не составная.
        std::uint32_t solePart = 0;
        std::vector<Sensor> sensors{};
    };

    // Триггер, для которого строится датчик.
    struct PendingSensor {
        PlacedShape placed;
        BodyPart part;
        BodyPose offset;
        bool detectStatic = false;
    };

    // Хозяин тела Jolt и датчик ли оно.
    struct JoltBodyOwner {
        NodeId owner = 0;
        bool sensor = false;
    };

    void rebuild(NodeId owner);
    void addSensor(
        const Node& node,
        NodeId owner,
        const BodyPose& pose,
        PendingSensor& trigger,
        const MeshShapeSource& meshes,
        Body& body
    );
    void remove(NodeId owner);
    void destroy(const Body& body);
    void destroyJoltBody(JPH::BodyID id);
    // Узел, телу которого принадлежит коллайдер на node.
    NodeId ownerOf(const Node& node) const;
    // Ревизии трансформов от узлов дочерних коллайдеров до хозяина.
    std::vector<std::uint64_t> childRevisionsOf(const Node& owner) const;
    // Пишет положение тела в узел и запоминает, что в узле получилось.
    void writePose(Node& node, Body& body, const BodyPose& pose);
    void sortByDepth();
    // Сообщение в лог об объекте id один раз за жизнь мира.
    bool firstWarning(std::uint64_t id);

    Scene& m_scene;
    JPH::BodyInterface& m_interface;
    MeshShapeCache& m_meshShapes;
    PhysicsMaterialCache& m_materials;
    ResourceCache& m_resources;
    // Материал по умолчанию, с которым построены тела.
    ResourceId m_defaultMaterial = 0;
    std::unordered_set<std::uint64_t> m_warned;
    std::unordered_map<NodeId, Body> m_bodies;
    // Хозяева тел в порядке глубины узлов.
    std::vector<NodeId> m_order;
    // Коллайдер - узел-хозяин его тела.
    std::unordered_map<ComponentId, NodeId> m_colliderOwners;
    std::unordered_map<std::uint32_t, JoltBodyOwner> m_byJoltId;
    // Коллайдер - тело Jolt, в форме которого он сейчас.
    std::unordered_map<ComponentId, JPH::BodyID> m_colliderBodies;
    std::vector<NodeId> m_dirtyBranches;
    std::unordered_set<NodeId> m_dirtyOwners;
};

} // namespace yqlora
