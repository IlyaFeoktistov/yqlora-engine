#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <cstddef>
#include <cstdint>

namespace yqlora {

class CharacterController;
class Collider;
class Joint;
class PhysicsChain;
class PhysicsWorld;
class RigidBody;

// Закрытая часть мира для компонентов физики движка (тела, коллайдеры) и
// тестов движка: кода игры она не касается.
struct YQLORA_API PhysicsWorldAccess {
    // Создаёт мир Jolt, если его ещё нет; так делает первое тело.
    static void ensureWorld(PhysicsWorld& world);
    // Компонент физики вошёл в сцену или изменился: его тело перестроится
    // перед следующим шагом; мир создаётся, если его нет. Вне сцены - ничего.
    static void changed(Collider& collider);
    static void changed(RigidBody& body);
    static void changed(CharacterController& controller);
    static void changed(Joint& joint);
    static void changed(PhysicsChain& chain);
    // Компонент физики уходит из сцены.
    static void left(Collider& collider);
    static void left(RigidBody& body);
    static void left(CharacterController& controller);
    static void left(Joint& joint);
    static void left(PhysicsChain& chain);
    // У скрепления сменились пределы, пружины или мотор, у цепи тел -
    // мышцы: они применяются к готовым перед следующим шагом.
    static void tuned(Joint& joint);
    static void tuned(PhysicsChain& chain);

    // Для проверок: число тел и масса тела узла-хозяина (0 - тела нет).
    // Тела перестраиваются перед шагом, поэтому числа - на последний шаг.
    static std::size_t bodyCount(const PhysicsWorld& world);
    static float bodyMass(const PhysicsWorld& world, NodeId owner);
    // Сколько форм из мешей приготовила среда мира.
    static std::uint64_t meshShapesCooked(const PhysicsWorld& world);
    // Перестраивает изменённые тела сейчас, как перед шагом.
    static void flush(PhysicsWorld& world);
};

} // namespace yqlora
