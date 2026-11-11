#include "yqlora/platform/FileDialogs.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/platform/Window.hpp"

#include <SDL3/SDL.h>

#include <mutex>
#include <string>

namespace yqlora {

struct FileDialogs::Dialog {
    PathCallback callback;
    std::mutex mutex;
    bool answered = false;
    std::optional<std::filesystem::path> path;
};

void FileDialogs::onAnswer(void* userdata, const char* const* files, int /*filter*/)
{
    auto* dialog = static_cast<Dialog*>(userdata);
    std::optional<std::filesystem::path> path;
    if (!files) {
        log::warn("File dialog is not available: {}", SDL_GetError());
    } else if (files[0]) {
        path = fromUtf8(files[0]);
    }
    const std::lock_guard lock(dialog->mutex);
    dialog->path = std::move(path);
    dialog->answered = true;
}

FileDialogs::FileDialogs(Window& window)
    : m_window(window)
{
}

FileDialogs::~FileDialogs()
{
    // Диалог мог остаться открытым: SDL ещё может вызвать его callback.
    // Тогда отдаём состояние ему насовсем, чтобы указатель не повис.
    if (m_dialog) {
        const std::lock_guard lock(m_dialog->mutex);
        if (!m_dialog->answered) {
            static_cast<void>(m_dialog.release());
        }
    }
}

FileDialogs::Dialog* FileDialogs::begin(PathCallback callback)
{
    if (m_dialog) {
        return nullptr;
    }
    m_dialog = std::make_unique<Dialog>();
    m_dialog->callback = std::move(callback);

    return m_dialog.get();
}

void FileDialogs::showFolderDialog(const std::filesystem::path& start, PathCallback callback)
{
    Dialog* dialog = begin(std::move(callback));
    if (!dialog) {
        return;
    }
    const std::string startUtf8 = toUtf8(start);
    SDL_ShowOpenFolderDialog(onAnswer, dialog, m_window.sdlWindow(), startUtf8.c_str(), false);
}

void FileDialogs::showFileDialog(const std::filesystem::path& start, PathCallback callback)
{
    Dialog* dialog = begin(std::move(callback));
    if (!dialog) {
        return;
    }
    // Без фильтров: у программ на Linux расширения нет.
    const std::string startUtf8 = toUtf8(start);
    SDL_ShowOpenFileDialog(onAnswer, dialog, m_window.sdlWindow(), nullptr, 0, startUtf8.c_str(), false);
}

void FileDialogs::deliver()
{
    if (!m_dialog) {
        return;
    }
    {
        const std::lock_guard lock(m_dialog->mutex);
        if (!m_dialog->answered) {
            return;
        }
    }
    // Callback может открыть новый диалог, поэтому сначала освобождаем место.
    const std::unique_ptr<Dialog> dialog = std::move(m_dialog);
    dialog->callback(std::move(dialog->path));
}

} // namespace yqlora
