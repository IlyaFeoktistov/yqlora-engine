// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/JoltContacts.hpp"

#include "yqlora/physics/CollisionIgnores.hpp"
#include "yqlora/physics/PhysicsBodies.hpp"

#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/physics/JoltShapes.hpp"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#include <utility>

namespace yqlora {

namespace {

// Масса неподвижного и кинематического тела бесконечна: обратная - 0.
float inverseMassOf(const JPH::Body& body)
{
    return body.IsDynamic() ? body.GetMotionProperties()->GetInverseMass() : 0.0f;
}

JPH::Vec3 velocityAt(const JPH::Body& body, JPH::RVec3Arg point)
{
    return body.IsStatic() ? JPH::Vec3::sZero() : body.GetPointVelocity(point);
}

} // namespace

JoltContacts::JoltContacts(const PhysicsBodies& bodies, const CollisionIgnores& ignores)
    : m_bodies(bodies)
    , m_ignores(ignores)
{
}

JPH::ValidateResult JoltContacts::OnContactValidate(
    const JPH::Body& first,
    const JPH::Body& second,
    JPH::RVec3Arg /*baseOffset*/,
    const JPH::CollideShapeResult& collision
)
{
    if (m_ignores.empty()) {
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }
    const auto a = m_bodies.sideOf(
        first.GetID().GetIndexAndSequenceNumber(),
        bodyPartOf(*first.GetShape(), collision.mSubShapeID1)
    );
    const auto b = m_bodies.sideOf(
        second.GetID().GetIndexAndSequenceNumber(),
        bodyPartOf(*second.GetShape(), collision.mSubShapeID2)
    );
    if (a && b
        && (m_ignores.ignored(a->part.collider, b->part.collider) || m_ignores.bodiesIgnored(a->owner, b->owner))) {
        return JPH::ValidateResult::RejectContact;
    }

    return JPH::ValidateResult::AcceptContact;
}

void JoltContacts::OnContactAdded(
    const JPH::Body& first,
    const JPH::Body& second,
    const JPH::ContactManifold& manifold,
    JPH::ContactSettings& settings
)
{
    record(first, second, manifold, settings, true);
}

void JoltContacts::OnContactPersisted(
    const JPH::Body& first,
    const JPH::Body& second,
    const JPH::ContactManifold& manifold,
    JPH::ContactSettings& settings
)
{
    record(first, second, manifold, settings, false);
}

void JoltContacts::record(
    const JPH::Body& first,
    const JPH::Body& second,
    const JPH::ContactManifold& manifold,
    const JPH::ContactSettings& settings,
    bool added
)
{
    RawContact contact{
        .first = first.GetID().GetIndexAndSequenceNumber(),
        .second = second.GetID().GetIndexAndSequenceNumber(),
        .firstPart = bodyPartOf(*first.GetShape(), manifold.mSubShapeID1),
        .secondPart = bodyPartOf(*second.GetShape(), manifold.mSubShapeID2),
        .added = added,
        .points = {},
        .normal = fromJolt(manifold.mWorldSpaceNormal),
    };
    for (JPH::uint i = 0; i < manifold.mRelativeContactPointsOn1.size(); ++i) {
        contact.points.push_back(fromJolt(JPH::Vec3(manifold.GetWorldSpaceContactPointOn1(i))));
    }
    const JPH::RVec3 point = manifold.GetWorldSpaceContactPointOn1(0);
    const JPH::Vec3 relative = velocityAt(first, point) - velocityAt(second, point);
    contact.relativeVelocity = fromJolt(relative);
    // Сближение по нормали: нормаль смотрит из первого тела во второе.
    const float approach = relative.Dot(manifold.mWorldSpaceNormal);
    const float inverseMasses = inverseMassOf(first) + inverseMassOf(second);
    if (added && approach > 0.0f && inverseMasses > 0.0f && !settings.mIsSensor) {
        contact.impulse = (1.0f + settings.mCombinedRestitution) * approach / inverseMasses;
    }
    const std::lock_guard lock(m_mutex);
    m_contacts.push_back(std::move(contact));
}

std::vector<RawContact> JoltContacts::take()
{
    const std::lock_guard lock(m_mutex);

    return std::exchange(m_contacts, {});
}

} // namespace yqlora
