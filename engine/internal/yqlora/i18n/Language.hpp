#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/core/Settings.hpp"
#include "yqlora/i18n/Localization.hpp"

#include <filesystem>
#include <string_view>

namespace yqlora {

// Язык интерфейса: переводы и выбор пользователя, который переживает
// перезапуск. Пока объект жив, через него работает tr().
class YQLORA_API Language {
public:
    // При первом запуске (или если сохранённого языка больше нет) берётся
    // язык системы и сразу запоминается.
    Language(const std::filesystem::path& localesDirectory, const std::filesystem::path& settingsFile);
    ~Language();

    Language(const Language&) = delete;
    Language& operator=(const Language&) = delete;

    const Localization& localization() const
    {
        return m_localization;
    }

    // Английские тексты при любом выбранном языке: поиск в окнах редактора
    // находит тип и по английскому слову.
    const Localization& english() const
    {
        return m_english;
    }

    // Переключает язык и запоминает выбор.
    bool set(std::string_view language);

private:
    Settings m_settings;
    Localization m_localization;
    Localization m_english;
};

} // namespace yqlora
