#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <cstdint>

namespace yqlora {

// ID компонента: уникален среди компонентов своего узла и не меняется за его
// жизнь, 0 означает, что компонента нет. Вместе с ID узла находит компонент
// в сцене (ссылки на компоненты, переопределения префабов).
using ComponentId = std::uint64_t;

// Новый ID узла или компонента: 64 случайных бита, никогда не 0, так что
// совпадение у объектов из разных сцен практически исключено.
YQLORA_API std::uint64_t newObjectId();

// ID объекта экземпляра префаба из ID экземпляра и ID объекта в префабе:
// одинаковый на любой машине и при каждой загрузке, никогда не 0. Так ссылки
// внутрь экземпляра переживают изменения префаба (см. prefabs.md).
constexpr std::uint64_t deriveId(std::uint64_t instance, std::uint64_t source)
{
    // splitmix64 от ID экземпляра, смешанного с перемешанным ID источника:
    // порядок аргументов важен, deriveId(a, b) != deriveId(b, a).
    const auto mix = [](std::uint64_t value) {
        value += 0x9E3779B97F4A7C15ULL;
        value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;

        return value ^ (value >> 31U);
    };
    const std::uint64_t id = mix(instance ^ mix(source));

    return id != 0 ? id : 1;
}

} // namespace yqlora
