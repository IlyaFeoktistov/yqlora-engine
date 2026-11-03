#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/resource/ResourceMeta.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

// ID и пути ресурсов проекта. ID хранится в файле <ресурс>.meta рядом с самим
// ресурсом, поэтому переживает переименование и перенос, если .meta переезжает
// вместе с файлом (так делает редактор). Все пути - относительно папки проекта.
//
// В файле бывают вложенные ресурсы (меши модели). У каждого свой ID в .meta
// файла под ключом, который даёт загрузчик ("mesh:Door"): ID создаётся один
// раз и не вычисляется из ключа. Запись вложенного ресурса, которого в файле
// больше нет, остаётся: вернётся ресурс - вернётся и его ID.
class YQLORA_API ResourceDatabase {
public:
    static constexpr std::string_view metaExtension()
    {
        return ".meta";
    }

    // Обходит папку проекта. Файлу без .meta (или с непрочитанной .meta)
    // создаёт .meta с новым ID. Если ID файла повторяется (файл скопировали
    // вместе с .meta), копия получает новый ID и новые вложенные ID. Скрытые
    // файлы, пропущенные имена в корне (файл проекта, папка настроек) и файлы
    // с пропущенными расширениями (исходники) не ресурсы.
    void scan(
        const std::filesystem::path& root,
        std::span<const std::string_view> skipNames = {},
        std::span<const std::string_view> skipExtensions = {}
    );

    // Новый файл в проекте: .meta с новым ID. Если файл уже известен, его ID.
    ResourceId add(const std::filesystem::path& path);

    // Файл или папку уже переименовали: .meta переезжают следом, ID остаются.
    void moved(const std::filesystem::path& from, const std::filesystem::path& to);

    // Файл или папку уже удалили: удаляет их .meta и забывает ID, вложенные тоже.
    void removed(const std::filesystem::path& path);

    // Файл ресурса (и вложенного: файл, в котором он лежит). nullopt, если
    // такого ресурса в проекте нет.
    std::optional<std::filesystem::path> path(ResourceId id) const;

    // 0, если файл не ресурс проекта.
    ResourceId id(const std::filesystem::path& path) const;

    // ID файла, в котором лежит ресурс; у самого файла - он сам, 0 - не знаем.
    ResourceId fileOf(ResourceId id) const;

    // Ключ вложенного ресурса в его файле; у файла и неизвестного ID - пустой.
    std::string_view key(ResourceId id) const;

    // ID вложенных ресурсов файла file по их ключам, в том же порядке. Новые
    // ключи получают ID, и .meta файла записывается один раз. Пусто, если
    // файла нет.
    std::vector<ResourceId> assignSubresources(ResourceId file, std::span<const std::string> keys);

    // Все вложенные ресурсы файла, ключ -> ID, и те, которых в файле уже нет.
    const std::map<std::string, ResourceId>& subresources(ResourceId file) const;

    // Настройки импорта файла - объект JSON текстом ("{}", если их нет): их
    // читает загрузчик своего вида. Хранятся в .meta.
    const std::string& importSettings(ResourceId file) const;

    // Записывает настройки импорта в .meta. false, если файла нет, текст не
    // объект JSON или .meta не записалась.
    bool setImportSettings(ResourceId file, std::string json);

    std::size_t size() const
    {
        return m_paths.size();
    }

    // Все файлы-ресурсы проекта: ID и путь от папки проекта, по порядку ID.
    // Вложенных ресурсов здесь нет.
    const std::map<ResourceId, std::filesystem::path>& paths() const
    {
        return m_paths;
    }

private:
    struct Nested {
        ResourceId file = 0;
        std::string key;
    };

    void remember(ResourceId id, const std::filesystem::path& path, ResourceMeta meta);
    void forget(ResourceId id);
    bool writeMeta(ResourceId id) const;
    bool known(ResourceId id) const;
    ResourceId newId() const;

    std::filesystem::path m_root;
    std::map<ResourceId, std::filesystem::path> m_paths;
    std::map<std::filesystem::path, ResourceId> m_ids;
    std::map<ResourceId, ResourceMeta> m_meta;
    std::map<ResourceId, Nested> m_nested;
};

} // namespace yqlora
