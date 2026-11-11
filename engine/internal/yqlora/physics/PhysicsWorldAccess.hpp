#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <cstddef>
#include <cstdint>

namespace yqlora {

class Component;
class PhysicsWorld;

// Закрытая часть мира для компонентов физики движка (тела, коллайдеры) и
// тестов движка: кода игры она не касается.
struct YQLORA_API PhysicsWorldAccess {
    // Создаёт мир Jolt, если его ещё нет; так делает первое тело.
    static void ensureWorld(PhysicsWorld& world);
    // Компонент физики вошёл в сцену или изменился: его тело перестроится
    // перед следующим шагом; мир создаётся, если его нет. Вне сцены - ничего.
    static void changed(Component& component);
    // Компонент физики уходит из сцены.
    static void left(Component& component);

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
