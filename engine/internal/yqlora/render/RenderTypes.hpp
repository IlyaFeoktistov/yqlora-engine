#pragma once

namespace yqlora {

class TypeRegistry;

// Камера, источники света, MeshRenderer, RenderSystem и разделы настроек рендера. Типы сцены должны быть уже зарегистрированы.
void registerRenderTypes(TypeRegistry& registry);

} // namespace yqlora
