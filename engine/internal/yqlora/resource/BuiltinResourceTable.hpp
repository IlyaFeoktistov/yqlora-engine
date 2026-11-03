#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <functional>
#include <memory>
#include <span>
#include <string_view>

namespace yqlora {

// Встроенный ресурс любого вида: ID, вид данных, имя и как их сделать.
// Делается при первом обращении, поэтому встроенные шейдеры компилирует
// только тот, кому они нужны.
struct BuiltinResourceEntry {
    ResourceId id;
    std::string_view kind;
    // Английское имя, им ресурс подписан в редакторе: "cube", "Standard".
    std::string_view name;
    std::function<std::shared_ptr<const void>()> make;
};

// Все встроенные ресурсы: для кэша ресурсов и для выбора в редакторе.
YQLORA_API std::span<const BuiltinResourceEntry> builtinResourceTable();

// nullptr, если встроенного ресурса id нет.
YQLORA_API const BuiltinResourceEntry* findBuiltinResource(ResourceId id);

} // namespace yqlora
