#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/platform/MouseWrap.hpp"
#include "yqlora/render/GraphicsApi.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <string>

struct SDL_Window;
union SDL_Event;

namespace yqlora {

class Gamepads;
class InputFeeder;

inline constexpr int kDefaultWindowWidth = 1280;
inline constexpr int kDefaultWindowHeight = 720;

struct WindowDesc {
    std::string title;
    int width = kDefaultWindowWidth;
    int height = kDefaultWindowHeight;
    bool resizable = true;
    // Скрытое окно - только ради контекста графического API (тесты).
    bool visible = true;
    // От API зависит, с каким флагом SDL создаёт окно (OpenGL/Vulkan/...).
    GraphicsApi api = GraphicsApi::OpenGL;
};

struct WindowEvents {
    // Каждое событие ОС до того, как его разберёт окно: например, для слоя интерфейса.
    std::function<void(const SDL_Event&)> onEvent;
    std::function<void()> onCloseRequested;
    std::function<void(int width, int height)> onResized; // размер в пикселях
    std::function<void()> onFocusGained;
};

class YQLORA_API Window {
public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Обрабатывает все накопившиеся события ОС: клавиатуру и мышь передаёт
    // во ввод, геймпады - в gamepads, остальное в обработчики.
    void pollEvents(const WindowEvents& events, InputFeeder& input, Gamepads& gamepads);

    // Пока включено, курсор, дошедший до края окна, переносится к противоположному
    // краю, а смещение мыши считается без скачка. Так обзор камерой в редакторе
    // не упирается в край экрана, а курсор остаётся видно.
    //
    // Если система молча не переносит курсор (так в WSLg), перенос выключается
    // до конца работы: обзор работает, но у края экрана поворот останавливается.
    void setMouseWrapping(bool wrapping);

    bool mouseWrapping() const
    {
        return m_mouseWrapping;
    }

    void setTitle(const std::string& title);

    // Иконка окна: RGBA по байту на канал, строки сверху вниз.
    void setIcon(int width, int height, std::span<const std::uint8_t> rgba);

    // В тех же единицах, что положение мыши. На экранах с масштабом
    // меньше размера в пикселях.
    Vec2 sizeInPoints() const;

    int widthInPixels() const;
    int heightInPixels() const;

    // API, под которое создано окно (см. WindowDesc::api).
    GraphicsApi api() const
    {
        return m_api;
    }

    // Для бэкендов рендеринга: им нужно окно SDL, чтобы создать контекст/поверхность.
    SDL_Window* sdlWindow() const
    {
        return m_window;
    }

private:
    // Смещение мыши в режиме переноса; переносит курсор, если он у края.
    Vec2 wrappedMouseDelta(const Vec2& position);

    SDL_Window* m_window = nullptr;
    GraphicsApi m_api = GraphicsApi::OpenGL;
    MouseWrap m_mouseWrap;
    bool m_mouseWrapping = false;
    // Выяснилось, что система не переносит курсор: больше не пытаемся.
    bool m_warpUnsupported = false;
};

// Годится ли окно для API: окна SDL под OpenGL и под Vulkan создаются с
// разными флагами, и при смене API между ними окно приходится пересоздать.
YQLORA_API bool windowFitsApi(const Window& window, GraphicsApi api);

} // namespace yqlora
