#pragma once

#include "yqlora/core/Export.hpp"

#include <cstdint>

namespace yqlora {

// Константы объявлены вне класса GameTime: MinGW не умеет импортировать из dll
// статические поля класса, даже constexpr, и код, который берёт такое поле по
// ссылке, не компонуется.

// Фиксированный шаг по умолчанию; в игре он берётся из раздела Time (TimeSettings).
inline constexpr float kDefaultFixedStep = 1.0f / 60.0f;
// Больше шагов за кадр не делаем: иначе после долгого кадра физика
// отстаёт всё сильнее, каждый следующий кадр длиннее предыдущего.
inline constexpr int kMaxFixedStepsPerFrame = 8;

// Время игры одной сцены: идёт только пока сцену двигают (Scene::advance),
// с шкалой (0 - стоп, 0.5 - замедление) и накопителем фиксированного шага.
class YQLORA_API GameTime {
public:
    explicit GameTime(float fixedStep = kDefaultFixedStep);

    // Новый кадр длиной realDelta секунд по часам.
    void beginFrame(float realDelta);

    // Забирает из накопителя один фиксированный шаг; false, если не набралось.
    bool takeFixedStep();

    // С начала игры, по шкале.
    double elapsed() const
    {
        return m_elapsed;
    }

    // Этот кадр, по шкале.
    float delta() const
    {
        return m_delta;
    }

    // Этот кадр по часам: для того, что не должно замедляться вместе с игрой.
    float unscaledDelta() const
    {
        return m_unscaledDelta;
    }

    float fixedStep() const
    {
        return m_fixedStep;
    }

    void setFixedStep(float step);

    // Какая доля фиксированного шага набралась после последнего шага, от 0
    // до 1: насколько показанное должно уйти от последнего шага физики к
    // следующему (сглаживание).
    float fixedStepFraction() const
    {
        return m_accumulator / m_fixedStep;
    }

    float scale() const
    {
        return m_scale;
    }

    // Отрицательная шкала считается нулём: время назад не идёт.
    void setScale(float scale);

    // Сколько кадров прошло.
    std::uint64_t frame() const
    {
        return m_frame;
    }

private:
    float m_fixedStep;
    float m_scale = 1.0f;
    double m_elapsed = 0.0;
    float m_delta = 0.0f;
    float m_unscaledDelta = 0.0f;
    float m_accumulator = 0.0f;
    std::uint64_t m_frame = 0;
};

} // namespace yqlora
