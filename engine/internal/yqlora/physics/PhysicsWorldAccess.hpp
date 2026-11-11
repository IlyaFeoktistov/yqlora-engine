#pragma once

#include "yqlora/core/Export.hpp"

namespace yqlora {

class PhysicsWorld;

// Закрытая часть мира для компонентов физики движка (тела, коллайдеры):
// кода игры она не касается.
struct YQLORA_API PhysicsWorldAccess {
    // Создаёт мир Jolt, если его ещё нет; так делает первое тело.
    static void ensureWorld(PhysicsWorld& world);
};

} // namespace yqlora
