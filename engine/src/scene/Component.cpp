#include "yqlora/scene/Component.hpp"

#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/Scene.hpp"

#include <cassert>

namespace yqlora {

Component::Component()
    : m_id(newObjectId())
{
}

Component::~Component() = default;

std::weak_ptr<const void> Component::lifetime()
{
    if (!m_lifetime) {
        m_lifetime = std::make_shared<const char>();
    }

    return m_lifetime;
}

void Component::endLifetime()
{
    m_lifetime.reset();
}

void Component::setId(ComponentId id)
{
    assert(id != 0 && "setId: 0 means no component");
    m_id = id;
}

void Component::setEnabled(bool enabled)
{
    assert((!m_builtin || enabled) && "setEnabled: a builtin component cannot be disabled");
    if (m_enabled == enabled || m_builtin) {
        return;
    }
    m_enabled = enabled;
    Scene* scene = m_owner ? m_owner->scene() : nullptr;
    if (!scene) {
        return;
    }
    if (m_owner->activeInHierarchy()) {
        if (enabled) {
            scene->activate(*this);
        } else {
            scene->deactivate(*this);
        }
    }
    scene->branchChanged(*m_owner);
}

bool Component::activeAndEnabled() const
{
    return m_enabled && m_owner && m_owner->activeInHierarchy();
}

} // namespace yqlora
