#include "yqlora/editor/ui/core/EditorHost.hpp"

#include "yqlora/core/Application.hpp"

namespace yqlora::editor {

Renderer& EditorHost::renderer() const
{
    return m_app.renderer();
}

const Input& EditorHost::input() const
{
    return m_app.input();
}

Language& EditorHost::language() const
{
    return m_app.language();
}

void EditorHost::showFolderDialog(
    const std::filesystem::path& start,
    std::function<void(std::optional<std::filesystem::path>)> callback
) const
{
    m_app.showFolderDialog(start, std::move(callback));
}

} // namespace yqlora::editor
