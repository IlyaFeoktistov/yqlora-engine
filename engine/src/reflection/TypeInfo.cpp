#include "yqlora/reflection/TypeInfo.hpp"

#include "yqlora/project/SettingsSection.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/SceneSystem.hpp"

namespace yqlora {

namespace {

// Ключи перевода: type.<имя типа>.description и type.<имя типа>.name.
constexpr std::string_view kTypeKeyPrefix = "type.";
constexpr std::string_view kDescriptionKeySuffix = ".description";
constexpr std::string_view kNameKeySuffix = ".name";

std::string typeKey(const std::string& name, std::string_view suffix)
{
    std::string key;
    key.reserve(kTypeKeyPrefix.size() + name.size() + suffix.size());
    key.append(kTypeKeyPrefix).append(name).append(suffix);

    return key;
}

} // namespace

std::string TypeInfo::descriptionKey() const
{
    return typeKey(m_name, kDescriptionKeySuffix);
}

std::string TypeInfo::nameKey() const
{
    return typeKey(m_name, kNameKeySuffix);
}

std::unique_ptr<Node> TypeInfo::createNode() const
{
    return m_createNode ? m_createNode() : nullptr;
}

std::unique_ptr<Component> TypeInfo::createComponent() const
{
    return m_createComponent ? m_createComponent() : nullptr;
}

std::unique_ptr<SceneSystem> TypeInfo::createSystem(Scene& scene) const
{
    return m_createSystem ? m_createSystem(scene) : nullptr;
}

std::unique_ptr<SettingsSection> TypeInfo::createSettings() const
{
    return m_createSettings ? m_createSettings() : nullptr;
}

} // namespace yqlora
