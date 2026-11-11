#pragma once

#include "yqlora/core/Export.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

namespace yqlora {

class Window;

// Системные диалоги выбора файлов и папок поверх окна.
class YQLORA_API FileDialogs {
public:
    explicit FileDialogs(Window& window);
    ~FileDialogs();

    FileDialogs(const FileDialogs&) = delete;
    FileDialogs& operator=(const FileDialogs&) = delete;

    // Диалоги выбора папки и файла. Ответ придёт в callback из deliver(), в
    // главном потоке; nullopt, если выбор отменили или диалог недоступен.
    // Пока открыт один диалог, второй не открывается.
    using PathCallback = std::function<void(std::optional<std::filesystem::path>)>;
    void showFolderDialog(const std::filesystem::path& start, PathCallback callback);
    // start - папка или файл, с которого диалог начинает.
    void showFileDialog(const std::filesystem::path& start, PathCallback callback);

    // Отдаёт готовый ответ диалога. Вызывается раз в кадр.
    void deliver();

private:
    // Диалог SDL может ответить из другого потока: ответ кладётся сюда,
    // а отдаётся в deliver().
    struct Dialog;

    // Место под ответ нового диалога; nullptr, если один уже открыт.
    Dialog* begin(PathCallback callback);
    // Ответ SDL, из любого потока.
    static void onAnswer(void* userdata, const char* const* files, int filter);

    Window& m_window;
    std::unique_ptr<Dialog> m_dialog;
};

} // namespace yqlora
