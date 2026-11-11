#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/Record.hpp"
#include "yqlora/scene/Layers.hpp"

#include <array>
#include <vector>

namespace yqlora {

// Пара слоёв, которые не сталкиваются. Порядок слоёв в паре не важен.
struct LayerPair {
    int first = 0;
    int second = 0;

    bool operator==(const LayerPair&) const = default;
};

// Какие слои узлов сталкиваются друг с другом: kLayerCount на kLayerCount,
// по умолчанию всё со всем. Симметрична: пара говорит об обоих направлениях.
class YQLORA_API LayerCollisionMatrix {
public:
    LayerCollisionMatrix();

    // false для номера вне слоёв.
    bool collides(int first, int second) const;
    // Номер вне слоёв ничего не меняет.
    void setCollides(int first, int second, bool collides);
    // Слои, с которыми сталкивается layer; пусто для номера вне слоёв.
    LayerMask mask(int layer) const;

    // Пары, которые не сталкиваются, по возрастанию, меньший слой первым:
    // так матрица пишется в файл (по умолчанию список пуст).
    std::vector<LayerPair> ignoredPairs() const;
    // Остальные пары сталкиваются; пары вне слоёв отбрасываются, повторы безвредны.
    void setIgnoredPairs(const std::vector<LayerPair>& pairs);

    bool operator==(const LayerCollisionMatrix&) const = default;

private:
    std::array<LayerMask, kLayerCount> m_masks{};
};

} // namespace yqlora

YQLORA_RECORD(
    yqlora::LayerPair,
    field("first", &yqlora::LayerPair::first, PropertyHint::Layer),
    field("second", &yqlora::LayerPair::second, PropertyHint::Layer)
)
