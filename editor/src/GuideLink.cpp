#include "yqlora/editor/GuideLink.hpp"

#include <array>

namespace yqlora::editor {

namespace {

// TODO: ветка main, а не версия движка: руководство может опередить
// установленный движок. Когда появятся теги выпусков - ссылка на тег версии.
constexpr std::string_view kGuideBaseUrl = "https://github.com/IlyaFeoktistov/yqlora-engine/blob/main/docs/guide/";
constexpr std::string_view kPageExtension = ".md";
// Языки, на которые руководство переведено: страница <page>.<язык>.md.
constexpr std::array<std::string_view, 1> kTranslatedLanguages = {"ru"};

} // namespace

std::string guideUrl(std::string_view page, std::string_view language)
{
    std::string url = std::string(kGuideBaseUrl) + std::string(page);
    for (const std::string_view translated : kTranslatedLanguages) {
        if (language == translated) {
            url += "." + std::string(translated);
        }
    }

    return url + std::string(kPageExtension);
}

} // namespace yqlora::editor
