#include "yqlora/scene/GameTime.hpp"

#include <algorithm>
#include <cassert>

namespace yqlora {

GameTime::GameTime(float fixedStep)
    : m_fixedStep(fixedStep)
{
    assert(fixedStep > 0.0f && "GameTime: the fixed step must be positive");
}

void GameTime::beginFrame(float realDelta)
{
    m_unscaledDelta = std::max(realDelta, 0.0f);
    m_delta = m_unscaledDelta * m_scale;
    m_elapsed += m_delta;
    m_accumulator = std::min(m_accumulator + m_delta, m_fixedStep * static_cast<float>(kMaxFixedStepsPerFrame));
    ++m_frame;
}

bool GameTime::takeFixedStep()
{
    m_inFixedStep = m_accumulator >= m_fixedStep;
    if (!m_inFixedStep) {
        return false;
    }
    m_accumulator -= m_fixedStep;
    ++m_fixedSteps;

    return true;
}

void GameTime::setFixedStep(float step)
{
    assert(step > 0.0f && "GameTime: the fixed step must be positive");
    m_fixedStep = step;
}

void GameTime::setScale(float scale)
{
    m_scale = std::max(scale, 0.0f);
}

} // namespace yqlora
