#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/physics/BodyPose.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/NodeId.hpp"
#include "yqlora/scene/ObjectId.hpp"

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/MotionType.h>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace JPH {

class BodyInterface;

} // namespace JPH

namespace yqlora {

class Component;
class MeshShapeCache;
class Node;
class ResourceCache;
class Scene;

// Тела Jolt вслед за деревом сцены. Хозяин тела - узел: с RigidBody (тело
// из коллайдеров узла и потомков до следующего RigidBody) или с
// коллайдерами без RigidBody над ними (неявное неподвижное тело из
// коллайдеров этого узла). Изменения отмечаются сразу, а тела
// перестраиваются в flush, перед шагом. Устройство - docs/architecture/physics.md.
class PhysicsBodies {
public:
    PhysicsBodies(Scene& scene, JPH::BodyInterface& bodies, MeshShapeCache& meshShapes);
    ~PhysicsBodies();

    PhysicsBodies(const PhysicsBodies&) = delete;
    PhysicsBodies& operator=(const PhysicsBodies&) = delete;

    // В ветке node мог смениться хозяин коллайдеров или состав тел.
    void branchChanged(const Node& node);
    // Коллайдер или RigidBody уходит из сцены: его тело перестроится.
    void componentLeft(const Component& component);

    // Перестраивает изменённые тела и те, у которых дочерний коллайдер
    // сдвинулся относительно узла тела, сменился масштаб узла тела или
    // перевыгружен меш коллайдера.
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

    // Тело узла-хозяина; невалидный ID, если у узла тела нет.
    JPH::BodyID bodyOf(NodeId owner) const;

private:
    struct Body {
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
    };

    void rebuild(NodeId owner);
    void remove(NodeId owner);
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
    ResourceCache& m_resources;
    std::unordered_set<std::uint64_t> m_warned;
    std::unordered_map<NodeId, Body> m_bodies;
    // Хозяева тел в порядке глубины узлов.
    std::vector<NodeId> m_order;
    // Коллайдер - узел-хозяин его тела.
    std::unordered_map<ComponentId, NodeId> m_colliderOwners;
    std::vector<NodeId> m_dirtyBranches;
    std::unordered_set<NodeId> m_dirtyOwners;
};

} // namespace yqlora
