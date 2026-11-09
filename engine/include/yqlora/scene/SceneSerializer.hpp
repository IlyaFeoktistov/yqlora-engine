#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceKinds.hpp"
#include "yqlora/scene/Node.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace yqlora {

class Scene;
class TypeRegistry;

// Сохраняет сцену в JSON и загружает обратно. Узлы и компоненты создаются
// по именам типов из реестра, значения читаются и пишутся через их свойства.
class YQLORA_API SceneSerializer {
public:
    // Вид ресурса файла сцены для ссылок на него. Кэш ресурсов читает файл
    // сцены в SceneData (для сборки игры и префабов), а сцену из него строит
    // SceneSerializer.
    static constexpr std::string_view kResourceKind = kinds::kScene;

    // Расширение файлов сцен.
    static constexpr std::string_view fileExtension()
    {
        return ".yscene";
    }

    explicit SceneSerializer(const TypeRegistry& registry);

    const TypeRegistry& registry() const
    {
        return m_registry;
    }

    std::string toJson(const Scene& scene) const;

    // При ошибке возвращает false и оставляет сцену как была.
    bool fromJson(Scene& scene, std::string_view json) const;

    bool save(const Scene& scene, const std::filesystem::path& file) const;
    bool load(Scene& scene, const std::filesystem::path& file) const;

    // Компоненты, тип которых пропал при загрузке (MissingComponent), а теперь
    // снова есть в реестре (загрузили модуль игры), становятся настоящими на
    // том же месте со своими данными. Не во время кадра. Возвращает, сколько ожило.
    std::size_t reviveMissing(Scene& scene) const;

private:
    struct Format;
    // Ветка в формате файла для префабов, во внутреннем API (SceneBranches.hpp).
    friend struct SceneBranches;

    // ID узла и компонента из файла: их setId закрыт для всех, кроме сериализатора.
    static void assignId(Node& node, NodeId id);
    static void assignId(Component& component, ComponentId id);

    const TypeRegistry& m_registry;
};

} // namespace yqlora
