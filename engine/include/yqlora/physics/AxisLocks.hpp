#pragma once

#include "yqlora/reflection/Record.hpp"

namespace yqlora {

// Запертые оси: по ним (или вокруг них) не движутся. У тела - оси мира
// (игра в плоскости, дверь, которая только поворачивается), у скрепления с
// шестью осями - оси его узла.
struct AxisLocks {
    bool x = false;
    bool y = false;
    bool z = false;

    bool operator==(const AxisLocks&) const = default;
};

} // namespace yqlora

YQLORA_RECORD(
    yqlora::AxisLocks,
    field("x", &yqlora::AxisLocks::x),
    field("y", &yqlora::AxisLocks::y),
    field("z", &yqlora::AxisLocks::z)
)
