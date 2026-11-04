#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/rhi/CommandList.hpp"

#include <functional>
#include <string_view>

namespace yqlora {

struct RenderContext;

// Задаёт открытому проходу набор материала: блок Material (свой на этот
// кадр, если есть замены) и текстуры материала или замен - только те, что
// объявил привязанный шейдер shader (вариант шейдера может не пользоваться
// частью полей, и тогда их нет в его отражении). Замену, которая не подходит
// шейдеру, пропускает и сообщает о ней misfit(имя поля).
YQLORA_API void bindMaterial(
    rhi::CommandList& commands,
    const RenderContext& context,
    const rhi::Shader& shader,
    const FrameMaterial& material,
    const FrameOverrides* overrides,
    const std::function<void(std::string_view)>& misfit = {}
);

} // namespace yqlora
