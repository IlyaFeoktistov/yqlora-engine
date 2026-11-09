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
    if (m_owner && m_owner->activeInHierarchy() && m_owner->scene()) {
        if (enabled) {
            m_owner->scene()->activate(*this);
        } else {
            m_owner->scene()->deactivate(*this);
        }
    }
}

bool Component::activeAndEnabled() const
{
    return m_enabled && m_owner && m_owner->activeInHierarchy();
}

} // namespace yqlora
