#pragma once

#include <filesystem>
#include <functional>
#include <optional>

namespace yqlora {
class Application;
class Input;
class Language;
class Renderer;
} // namespace yqlora

namespace yqlora::editor {

// То, что панелям и меню редактора нужно от приложения, и ничего больше.
class EditorHost {
public:
    explicit EditorHost(Application& app)
        : m_app(app)
    {
    }

    Renderer& renderer() const;
    const Input& input() const;
    Language& language() const;

    void showFolderDialog(
        const std::filesystem::path& start,
        std::function<void(std::optional<std::filesystem::path>)> callback
    ) const;
    void showFileDialog(
        const std::filesystem::path& start,
        std::function<void(std::optional<std::filesystem::path>)> callback
    ) const;

private:
    Application& m_app;
};

} // namespace yqlora::editor
