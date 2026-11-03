#pragma once

#include "yqlora/core/Export.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

// Что загрузчику нужно знать о файле.
struct LoadRequest {
    // Полный путь к файлу.
    std::filesystem::path file;
    // Тот же файл относительно папки проекта: для сообщений и путей подключений.
    std::filesystem::path relative{};
    // Настройки импорта из .meta, объект; чего в нём нет - по умолчанию.
    nlohmann::json import = nlohmann::json::object();
};

// Один ресурс из файла. Пустой ключ - ресурс самого файла (ID из его .meta),
// остальные - вложенные ресурсы (меши внутри модели) с ключом от
// SubresourceKeys: по нему .meta находит их ID.
struct LoadedResource {
    std::string key;
    // Вид данных: T::kResourceKind у типа данных (MeshData и другие).
    std::string_view kind;
    std::shared_ptr<const void> data;
};

// Ресурсы файла или текст ошибки, если файл не прочитался.
struct LoadResult {
    std::vector<LoadedResource> resources{};
    std::string error{};
    // Полные пути файлов, прочитанных кроме самого файла (подключения
    // шейдера), и при ошибке тоже: изменился любой - файл перечитывается.
    std::vector<std::filesystem::path> dependencies{};
    // Поля, которые загрузчик записывает в настройки импорта .meta (версия
    // профиля шейдера); остальные настройки остаются.
    nlohmann::json importUpdate = nlohmann::json::object();
};

// Ключи вложенных ресурсов одного файла: "mesh:Door", у второго Door -
// "mesh:Door#1", у элемента без имени - "mesh:#2" по его индексу в файле.
// Ключ, уже выданный в этом файле, получает следующий номер, поэтому ключи
// не повторяются, даже если "#" есть в самом имени. Ключи зависят только от
// содержимого файла: перевыгрузка того же файла даёт те же ключи.
class YQLORA_API SubresourceKeys {
public:
    std::string next(std::string_view kind, std::string_view name, std::size_t indexInFile);

private:
    std::set<std::string, std::less<>> m_used;
};

// Ключ без вида, с номером: "mesh:Door#1" -> "Door#1", "mesh:#2" -> "#2".
YQLORA_API std::string_view subresourceLocalKey(std::string_view key);

// Имя элемента файла по его ключу, без вида и номера: "mesh:Door#1" ->
// "Door". У элемента без имени - пустое.
YQLORA_API std::string_view subresourceName(std::string_view key);

template <typename T>
LoadedResource makeLoadedResource(std::string key, T data)
{
    return {std::move(key), T::kResourceKind, std::make_shared<const T>(std::move(data))};
}

inline LoadResult loadError(std::string message)
{
    return {{}, std::move(message)};
}

// Читает файлы одного вида (glTF, картинки) в данные на стороне CPU. Новый
// вид ресурса - новый загрузчик, кэш и остальной движок не меняются.
// Загрузчик не бросает исключений: ошибка возвращается в LoadResult::error.
class YQLORA_API ResourceLoader {
public:
    virtual ~ResourceLoader() = default;

    // Расширения файлов в нижнем регистре, с точкой: ".glb".
    virtual std::span<const std::string_view> extensions() const = 0;

    // Вид ресурса самого файла (MeshData::kResourceKind и другие): по нему
    // редактор, не читая файл, знает, что в нём, например для списка текстур.
    virtual std::string_view kind() const = 0;

    // Виды вложенных ресурсов, которые бывают в таких файлах (меши модели):
    // по ним редактор знает, какие файлы читать для списка мешей.
    virtual std::span<const std::string_view> nestedKinds() const
    {
        return {};
    }

    virtual LoadResult load(const LoadRequest& request) const = 0;
};

} // namespace yqlora
