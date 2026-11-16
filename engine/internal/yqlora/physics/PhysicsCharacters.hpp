#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/math/Math.hpp"
#include "yqlora/scene/NodeId.hpp"
#include "yqlora/scene/ObjectId.hpp"

#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace JPH {

class BroadPhaseLayerInterface;
class ObjectLayerPairFilter;
class ObjectVsBroadPhaseLayerFilter;
class PhysicsSystem;
class TempAllocator;

} // namespace JPH

namespace yqlora {

class CharacterController;
class PhysicsBodies;

// Что персонаж знает после движения; без персонажа - всё нулевое.
struct CharacterState {
    bool grounded = false;
    Vec3 groundNormal{0.0f};
    Vec3 groundVelocity{0.0f};
    Vec3 velocity{0.0f};
};

// Персонажи мира сцены (CharacterController) на виртуальном персонаже
// Jolt. Узел - главный: перед движением, шагом и запросом персонаж встаёт
// туда, где узел (sync), а движение пишет в узел, куда персонаж дошёл.
// Своё внутреннее кинематическое тело персонажа видят другие тела,
// запросы и триггеры как тело его узла (PhysicsBodies::addCharacterBody).
class PhysicsCharacters final : private JPH::CharacterContactListener {
public:
    struct Filters {
        const JPH::ObjectVsBroadPhaseLayerFilter& broadPhase;
        const JPH::ObjectLayerPairFilter& pairs;
    };

    PhysicsCharacters(
        JPH::PhysicsSystem& system,
        PhysicsBodies& bodies,
        Filters filters,
        JPH::TempAllocator& allocator
    );
    ~PhysicsCharacters() override;

    PhysicsCharacters(const PhysicsCharacters&) = delete;
    PhysicsCharacters& operator=(const PhysicsCharacters&) = delete;

    // Контроллер вошёл в сцену или его капсула изменилась.
    void changed(CharacterController& controller);
    void left(const CharacterController& controller);

    // Создаёт, убирает и перестраивает персонажей по их контроллерам (слой,
    // участие в игре) и ставит их туда, где узлы.
    void sync();

    void move(CharacterController& controller, const Vec3& velocity, float deltaSeconds);
    // Пробует капсулу высоты height с центром center: false - ей нет
    // места, персонаж остаётся прежним. Без персонажа - true.
    bool tryCapsule(const CharacterController& controller, float height, const Vec3& center);
    CharacterState state(const CharacterController& controller) const;

private:
    struct Character {
        CharacterController* controller = nullptr;
        JPH::Ref<JPH::CharacterVirtual> virtualCharacter;
        // Слой узла, с которым построено внутреннее тело.
        int layer = 0;
        // Где персонаж оставил узел: отличие - узел сдвинули кодом или редактором.
        Vec3 placed{0.0f};
        Vec3 velocity{0.0f};
        bool dirty = true;
    };

    void build(Character& character);
    void destroy(Character& character);
    Character* ready(const CharacterController& controller);
    // Узлы тел, которых коснулся последний move, по разу.
    void emitHits(CharacterController& controller);

    void OnContactAdded(
        const JPH::CharacterVirtual* character,
        const JPH::CharacterContact& contact,
        JPH::CharacterContactSettings& settings
    ) override;

    JPH::PhysicsSystem& m_system;
    PhysicsBodies& m_bodies;
    Filters m_filters;
    JPH::TempAllocator& m_allocator;
    std::unordered_map<ComponentId, Character> m_characters;
    // Касания идущего move: тело Jolt и номер подфигуры.
    std::vector<std::pair<JPH::BodyID, JPH::SubShapeID>> m_hits;
    std::unordered_set<ComponentId> m_warned;
};

} // namespace yqlora
