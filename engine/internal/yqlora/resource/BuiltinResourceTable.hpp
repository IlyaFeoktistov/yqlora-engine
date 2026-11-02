#pragma once

#include "yqlora/resource/ResourceId.hpp"

#include <functional>
#include <memory>
#include <span>
#include <string_view>

namespace yqlora {

// Встроенный ресурс любого вида: ID, вид данных и как их сделать. Делается
// при первом обращении, поэтому встроенные шейдеры компилирует только тот,
// кому они нужны.
struct BuiltinResourceEntry {
    ResourceId id;
    std::string_view kind;
    std::function<std::shared_ptr<const void>()> make;
};

// Все встроенные ресурсы: меши и шейдеры.
std::span<const BuiltinResourceEntry> builtinResourceTable();

} // namespace yqlora
