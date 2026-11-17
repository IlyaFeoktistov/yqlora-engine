#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/physics/Collision.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/reflection/Record.hpp"
#include "yqlora/scene/Layers.hpp"

namespace yqlora {

class Collider;

// Что запрос к физике видит: коллайдеры на узлах слоёв mask; триггеры -
// только с includeTriggers.
struct QueryFilter {
    LayerMask mask = kAllLayers;
    bool includeTriggers = false;

    bool operator==(const QueryFilter&) const = default;
};

// Попадание луча или протянутой фигуры.
struct RaycastHit {
    // Узел тела (с RigidBody, с коллайдерами без него, с цепью тел) и
    // коллайдер; у звена цепи коллайдера нет, а link - номер звена.
    NodeRef node;
    ComponentRef<Collider> collider;
    int link = kNoLink;
    // Точка на коллайдере в мире и его нормаль в ней (наружу).
    Vec3 point{0.0f};
    Vec3 normal{0.0f};
    // Сколько луч или фигура прошли до попадания, м.
    float distance = 0.0f;

    bool operator==(const RaycastHit&) const = default;
};

} // namespace yqlora

YQLORA_RECORD(
    yqlora::QueryFilter,
    field("mask", &yqlora::QueryFilter::mask, PropertyHint::Layers),
    field("includeTriggers", &yqlora::QueryFilter::includeTriggers)
)

YQLORA_RECORD(
    yqlora::RaycastHit,
    field("node", &yqlora::RaycastHit::node),
    field("collider", &yqlora::RaycastHit::collider),
    field("link", &yqlora::RaycastHit::link),
    field("point", &yqlora::RaycastHit::point),
    field("normal", &yqlora::RaycastHit::normal),
    field("distance", &yqlora::RaycastHit::distance)
)
