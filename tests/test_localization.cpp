#include <doctest/doctest.h>

#include <yqlora/i18n/Localization.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using namespace yqlora;

TEST_CASE("Localization: loads nested keys and falls back to the key itself")
{
    Localization loc(YQLORA_LOCALES_DIR);
    REQUIRE(loc.load("ru"));
    CHECK(loc.language() == "ru");
    CHECK(loc.translate("language.name") == "Русский");
    CHECK(loc.translate("no.such.key") == "no.such.key");

    REQUIRE(loc.load("en"));
    CHECK(loc.translate("language.name") == "English");
}

TEST_CASE("Localization: unknown language keeps the previous one")
{
    Localization loc(YQLORA_LOCALES_DIR);
    REQUIRE(loc.load("en"));
    CHECK_FALSE(loc.load("xx"));
    CHECK(loc.language() == "en");
}

TEST_CASE("Localization: available languages are discovered from files")
{
    Localization loc(YQLORA_LOCALES_DIR);
    const auto languages = loc.availableLanguages();
    CHECK(languages == std::vector<std::string>{"en", "ru"});
}

TEST_CASE("Localization: pickLanguage matches system locales")
{
    const std::vector<std::string> available{"en", "ru"};

    CHECK(Localization::pickLanguage(std::vector<std::string>{"ru_RU"}, available) == "ru");
    CHECK(Localization::pickLanguage(std::vector<std::string>{"de", "ru-RU", "en"}, available) == "ru");
    CHECK(Localization::pickLanguage(std::vector<std::string>{"de"}, available) == "en");
    CHECK(Localization::pickLanguage(std::vector<std::string>{}, available) == "en");
}

namespace {

// Строки файла языка по ключам через точку.
void flatten(const nlohmann::json& json, const std::string& prefix, std::map<std::string, std::string>& out)
{
    for (const auto& [key, value] : json.items()) {
        const std::string path = prefix.empty() ? key : prefix + "." + key;
        if (value.is_object()) {
            flatten(value, path, out);
        } else if (value.is_string()) {
            out[path] = value.get<std::string>();
        }
    }
}

std::map<std::string, std::string> localeStrings(const std::string& language)
{
    std::ifstream file(std::filesystem::path(YQLORA_LOCALES_DIR) / (language + ".json"));
    std::map<std::string, std::string> strings;
    flatten(nlohmann::json::parse(file), {}, strings);

    return strings;
}

} // namespace

TEST_CASE("Localization: a translation keeps the places for values of the English text")
{
    const auto en = localeStrings("en");
    const auto ru = localeStrings("ru");
    for (const auto& [key, text] : en) {
        const auto translated = ru.find(key);
        if (translated == ru.end()) {
            continue;
        }
        CAPTURE(key);
        CHECK(std::ranges::count(translated->second, '{') == std::ranges::count(text, '{'));
    }
}

TEST_CASE("Localization: trFormat puts the values where the translation says")
{
    Localization loc(YQLORA_LOCALES_DIR);
    REQUIRE(loc.load("ru"));
    Localization::setActive(&loc);
    CHECK(trFormat("editor.typePicker.wrongNode", "Node3D") == "Только на узлах типа \"Node3D\"");
    // Ключа нет - текст как есть, подставлять некуда.
    CHECK(trFormat("no.such.key", 1) == "no.such.key");
    Localization::setActive(nullptr);
}
