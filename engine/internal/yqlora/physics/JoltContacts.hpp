#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/math/Math.hpp"

#include <Jolt/Physics/Collision/ContactListener.h>

#include <cstdint>
#include <mutex>
#include <vector>

namespace yqlora {

// Касание двух подфигур после шага, как его видел Jolt: тела по ID Jolt
// (BodyID::GetIndexAndSequenceNumber) и номера коллайдеров в них
// (bodyPartOf). Нормаль - куда сдвинуть второе тело из первого.
struct RawContact {
    std::uint32_t first = 0;
    std::uint32_t second = 0;
    std::uint32_t firstPart = 0;
    std::uint32_t secondPart = 0;
    // Касание нашлось в этом шаге впервые (OnContactAdded).
    bool added = false;
    std::vector<Vec3> points;
    Vec3 normal{0.0f};
    // Скорость первого тела относительно второго в первой точке.
    Vec3 relativeVelocity{0.0f};
    float impulse = 0.0f;
};

// Слушатель контактов мира. Jolt зовёт его из рабочих потоков, поэтому он
// только записывает касания в буфер под замком, а события выдаёт главный
// поток после шага (PhysicsContacts). Выходы он не слушает: касание,
// которого после шага нет, ищет сам PhysicsContacts.
class JoltContacts final : public JPH::ContactListener {
public:
    void OnContactAdded(
        const JPH::Body& first,
        const JPH::Body& second,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings
    ) override;

    void OnContactPersisted(
        const JPH::Body& first,
        const JPH::Body& second,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings
    ) override;

    // Касания шага; буфер пустеет. Главный поток, после шага.
    std::vector<RawContact> take();

private:
    void record(
        const JPH::Body& first,
        const JPH::Body& second,
        const JPH::ContactManifold& manifold,
        const JPH::ContactSettings& settings,
        bool added
    );

    std::mutex m_mutex;
    std::vector<RawContact> m_contacts;
};

} // namespace yqlora
