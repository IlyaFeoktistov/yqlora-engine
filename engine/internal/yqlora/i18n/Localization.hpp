#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/i18n/Translate.hpp"

#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

// Переводы текстов интерфейса. Файлы: <dir>/<lang>.json, вложенные объекты
// превращаются в ключи через точку: {"app": {"title": "..."}} -> "app.title".
// Имена и типы узлов сцены сюда не относятся, они всегда на английском.
class YQLORA_API Localization {
public:
    static constexpr std::string_view kFallbackLanguage = "en";

    explicit Localization(std::filesystem::path localesDir);

    // Загружает язык; отсутствующие ключи берутся из kFallbackLanguage.
    bool load(std::string_view language);

    // Перевод по ключу. Если ключа нет ни в одном языке, возвращается сам ключ,
    // чтобы пропущенный перевод было видно, а не получить пустую строку.
    std::string translate(std::string_view key) const;

    // Есть ли ключ в самом загруженном языке, без запасного английского.
    bool has(std::string_view key) const;

    const std::string& language() const
    {
        return m_language;
    }

    // Коды языков, для которых есть файл в localesDir, отсортированы.
    std::vector<std::string> availableLanguages() const;

    // Название языка на нём самом ("Русский"), из ключа language.name его файла.
    // Если файла или ключа нет, возвращается код.
    std::string displayName(std::string_view language) const;

    // Первый язык из preferred (в порядке предпочтения пользователя), который
    // есть в available. "ru_RU" и "ru-RU" сводятся к "ru".
    static std::string pickLanguage(std::span<const std::string> preferred, std::span<const std::string> available);

    // Экземпляр, через который работает tr(). Назначает Application.
    static void setActive(const Localization* localization);

private:
    using Strings = std::map<std::string, std::string, std::less<>>;

    bool loadFile(std::string_view language, Strings& out) const;

    std::filesystem::path m_dir;
    std::string m_language;
    Strings m_strings;
    Strings m_fallback;
};

} // namespace yqlora
