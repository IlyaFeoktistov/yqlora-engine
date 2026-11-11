#include "yqlora/physics/LayerCollisionMatrix.hpp"

namespace yqlora {

namespace {

bool isLayer(int layer)
{
    return layer >= 0 && layer < kLayerCount;
}

LayerMask bit(int layer)
{
    return LayerMask{1} << layer;
}

} // namespace

LayerCollisionMatrix::LayerCollisionMatrix()
{
    m_masks.fill(kAllLayers);
}

bool LayerCollisionMatrix::collides(int first, int second) const
{
    return isLayer(first) && isLayer(second) && inMask(m_masks[first], second);
}

void LayerCollisionMatrix::setCollides(int first, int second, bool collides)
{
    if (!isLayer(first) || !isLayer(second)) {
        return;
    }
    if (collides) {
        m_masks[first] |= bit(second);
        m_masks[second] |= bit(first);
    } else {
        m_masks[first] &= ~bit(second);
        m_masks[second] &= ~bit(first);
    }
}

LayerMask LayerCollisionMatrix::mask(int layer) const
{
    return isLayer(layer) ? m_masks[layer] : LayerMask{0};
}

std::vector<LayerPair> LayerCollisionMatrix::ignoredPairs() const
{
    std::vector<LayerPair> pairs;
    for (int first = 0; first < kLayerCount; ++first) {
        for (int second = first; second < kLayerCount; ++second) {
            if (!collides(first, second)) {
                pairs.push_back({first, second});
            }
        }
    }

    return pairs;
}

void LayerCollisionMatrix::setIgnoredPairs(const std::vector<LayerPair>& pairs)
{
    m_masks.fill(kAllLayers);
    for (const LayerPair& pair : pairs) {
        setCollides(pair.first, pair.second, false);
    }
}

} // namespace yqlora
