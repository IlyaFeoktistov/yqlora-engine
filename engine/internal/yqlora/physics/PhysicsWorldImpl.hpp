#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/physics/CollisionIgnores.hpp"
#include "yqlora/physics/JoltContacts.hpp"
#include "yqlora/physics/JoltLayers.hpp"
#include "yqlora/physics/PhysicsBodies.hpp"
#include "yqlora/physics/PhysicsChains.hpp"
#include "yqlora/physics/PhysicsCharacters.hpp"
#include "yqlora/physics/PhysicsContacts.hpp"
#include "yqlora/physics/PhysicsJoints.hpp"
#include "yqlora/physics/PhysicsQueries.hpp"
#include "yqlora/physics/PhysicsWorld.hpp"

#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>

namespace yqlora {

class PhysicsRuntime;
class PhysicsSettings;

// Мир Jolt сцены: то, что PhysicsWorld держит за указателем. Его части
// живут в своих файлах (запросы, управление телом), поэтому устройство -
// здесь, а не в PhysicsWorld.cpp.
struct PhysicsWorld::Impl {
    Impl(Scene& scene, PhysicsRuntime& physicsRuntime, const PhysicsSettings& settings);

    PhysicsRuntime& runtime;
    // Фильтры живут дольше системы: она хранит ссылки на них.
    JoltBroadPhaseLayers broadPhaseLayers;
    JoltObjectVsBroadPhase objectVsBroadPhase;
    JoltObjectPairs pairs;
    CollisionIgnores ignores;
    JoltContacts listener;
    JPH::PhysicsSystem system;
    // После системы: уходит раньше неё, убирая свои тела.
    PhysicsBodies bodies;
    // После тел: внутренние тела персонажей уходят раньше мира тел.
    PhysicsCharacters characters;
    // После тел: скрепления уходят раньше тел, на которые ссылаются.
    PhysicsJoints joints;
    // После тел: звенья уходят раньше мира тел, который знает их как звенья.
    PhysicsChains chains;
    PhysicsContacts contacts;
    PhysicsQueries queries;
    // Поколение материалов, которое мир уже видел (PhysicsMaterialCache::generation).
    std::uint64_t materials = 0;
    // Ошибки шага, о которых лог уже сказал: каждая - один раз.
    JPH::EPhysicsUpdateError reported = JPH::EPhysicsUpdateError::None;
};

// Части мира Jolt, нужные тем, кто управляет телами (RigidBody).
struct JoltWorld {
    JPH::PhysicsSystem* system = nullptr;
    PhysicsBodies* bodies = nullptr;

    explicit operator bool() const
    {
        return system != nullptr;
    }
};

struct PhysicsWorldJolt {
    // Мир сцены компонента с перестроенными телами; пусто - компонент не в
    // сцене.
    static JoltWorld worldOf(const Component& component);
    // Персонажи мира сцены компонента; nullptr - компонент не в сцене.
    static PhysicsCharacters* charactersOf(const Component& component);
    // Скрепления мира сцены компонента, если мир есть; nullptr - нет.
    static const PhysicsJoints* jointsOf(const Component& component);
    // Цепи тел мира сцены компонента, готовые к чтению; nullptr - мира нет.
    static const PhysicsChains* chainsOf(const Component& component);
};

} // namespace yqlora
