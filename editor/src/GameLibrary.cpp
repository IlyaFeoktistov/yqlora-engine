#include "yqlora/editor/GameLibrary.hpp"

#include "yqlora/scripting/GameModule.hpp"

namespace yqlora::editor {

GameLibrary::GameLibrary(GameModule& module, const std::filesystem::path& projectDirectory)
    : m_module(module)
    , m_copies(projectDirectory)
{
    m_copies.removeUnused();
}

GameLibrary::~GameLibrary()
{
    m_copies.removeUnused();
}

bool GameLibrary::load(const std::filesystem::path& library)
{
    const auto copy = m_copies.copy(library);
    if (!copy || !m_module.load(*copy)) {
        m_copies.removeUnused();

        return false;
    }
    m_library = library;
    m_copies.removeUnused(*copy);

    return true;
}

void GameLibrary::unload(std::span<Scene* const> scenes)
{
    m_module.unload(scenes);
    m_library.clear();
}

} // namespace yqlora::editor
