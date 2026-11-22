#include "yqlora/i18n/Language.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/platform/Platform.hpp"

#include <algorithm>
#include <string>

namespace yqlora {

Language::Language(const std::filesystem::path& localesDirectory, const std::filesystem::path& settingsFile)
    : m_settings(settingsFile)
    , m_localization(localesDirectory)
    , m_english(localesDirectory)
{
    m_english.load(Localization::kFallbackLanguage);
    m_settings.load();
    log::info("Settings: {}", toUtf8(settingsFile));
    Localization::setActive(&m_localization);

    const auto available = m_localization.availableLanguages();
    if (std::ranges::find(available, m_settings.language) != available.end()) {
        m_localization.load(m_settings.language);

        return;
    }
    set(Localization::pickLanguage(platform::preferredLanguages(), available));
}

Language::~Language()
{
    Localization::setActive(nullptr);
}

bool Language::set(std::string_view language)
{
    if (!m_localization.load(language)) {
        return false;
    }
    m_settings.language = std::string(language);
    m_settings.save();

    return true;
}

} // namespace yqlora
