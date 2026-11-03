#pragma once

#include "yqlora/editor/ModuleCopies.hpp"

#include <filesystem>
#include <span>

namespace yqlora {
class GameModule;
class Scene;
} // namespace yqlora

namespace yqlora::editor {

// Библиотека игры открытого проекта: загружается копия собранной библиотеки
// (ModuleCopies), выгружается и перезагружается. Сцен и истории правок не
// знает: что с ними делать вокруг перезагрузки, решает EditorSession.
class GameLibrary {
public:
    // Копии, оставшиеся от прошлого запуска, удаляются.
    GameLibrary(GameModule& module, const std::filesystem::path& projectDirectory);
    // Модуль к этому времени выгружен (unload): удаляются все копии.
    ~GameLibrary();

    GameLibrary(const GameLibrary&) = delete;
    GameLibrary& operator=(const GameLibrary&) = delete;

    // Загружает копию library, лишние копии удаляет. false, если копия не
    // сделалась или модуль не загрузился.
    bool load(const std::filesystem::path& library);

    // Уничтожает экземпляры типов модуля в scenes и выгружает его.
    void unload(std::span<Scene* const> scenes);

    // Собранная библиотека, копия которой загружена; пусто - модуль не загружен.
    const std::filesystem::path& library() const
    {
        return m_library;
    }

private:
    GameModule& m_module;
    ModuleCopies m_copies;
    std::filesystem::path m_library;
};

} // namespace yqlora::editor
