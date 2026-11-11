#pragma once

namespace yqlora {

class TypeRegistry;

// PhysicsWorld и раздел настроек Physics. Типы сцены должны быть уже зарегистрированы.
void registerPhysicsTypes(TypeRegistry& registry);

} // namespace yqlora
