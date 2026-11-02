#include "yqlora/platform/Window.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/platform/Gamepads.hpp"
#include "yqlora/platform/InputFeeder.hpp"

#include <SDL3/SDL.h>

#include <optional>
#include <stdexcept>

namespace yqlora {

namespace {

SDL_WindowFlags apiFlag(GraphicsApi api)
{
    switch (api) {
    case GraphicsApi::OpenGL: return SDL_WINDOW_OPENGL;
    case GraphicsApi::Vulkan: return SDL_WINDOW_VULKAN;
    case GraphicsApi::Direct3D12: return 0;
    }

    return 0;
}

Key offsetKey(Key first, int offset)
{
    return static_cast<Key>(static_cast<int>(first) + offset);
}

Key toKey(SDL_Scancode scancode)
{
    // Буквы и F1-F12 идут подряд и в SDL, и у нас.
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        return offsetKey(Key::A, scancode - SDL_SCANCODE_A);
    }
    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) {
        return offsetKey(Key::F1, scancode - SDL_SCANCODE_F1);
    }
    // Цифры в SDL идут от 1 до 9, а 0 после них.
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        return offsetKey(Key::Num1, scancode - SDL_SCANCODE_1);
    }

    switch (scancode) {
    case SDL_SCANCODE_0: return Key::Num0;
    case SDL_SCANCODE_ESCAPE: return Key::Escape;
    case SDL_SCANCODE_TAB: return Key::Tab;
    case SDL_SCANCODE_SPACE: return Key::Space;
    case SDL_SCANCODE_RETURN: return Key::Enter;
    case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
    case SDL_SCANCODE_DELETE: return Key::Delete;
    case SDL_SCANCODE_LSHIFT: return Key::LeftShift;
    case SDL_SCANCODE_RSHIFT: return Key::RightShift;
    case SDL_SCANCODE_LCTRL: return Key::LeftCtrl;
    case SDL_SCANCODE_RCTRL: return Key::RightCtrl;
    case SDL_SCANCODE_LALT: return Key::LeftAlt;
    case SDL_SCANCODE_RALT: return Key::RightAlt;
    case SDL_SCANCODE_UP: return Key::Up;
    case SDL_SCANCODE_DOWN: return Key::Down;
    case SDL_SCANCODE_LEFT: return Key::Left;
    case SDL_SCANCODE_RIGHT: return Key::Right;
    default: return Key::Unknown;
    }
}

// Боковые кнопки мыши пока не поддерживаем.
std::optional<MouseButton> toMouseButton(std::uint8_t sdlButton)
{
    switch (sdlButton) {
    case SDL_BUTTON_LEFT: return MouseButton::Left;
    case SDL_BUTTON_RIGHT: return MouseButton::Right;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
    default: return std::nullopt;
    }
}

} // namespace

bool windowFitsApi(const Window& window, GraphicsApi api)
{
    return apiFlag(window.api()) == apiFlag(api);
}

Window::Window(const WindowDesc& desc)
    : m_api(desc.api)
{
    SDL_WindowFlags flags = apiFlag(desc.api) | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (desc.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (!desc.visible) {
        flags |= SDL_WINDOW_HIDDEN;
    }
    m_window = SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height, flags);
    if (!m_window) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
}

Window::~Window()
{
    SDL_DestroyWindow(m_window);
}

void Window::pollEvents(const WindowEvents& events, InputFeeder& input, Gamepads& gamepads)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (events.onEvent) {
            events.onEvent(event);
        }
        if (gamepads.handleEvent(event, input)) {
            continue;
        }
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (events.onCloseRequested) {
                events.onCloseRequested();
            }
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            if (events.onResized) {
                events.onResized(event.window.data1, event.window.data2);
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: input.releaseAll(); break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if (events.onFocusGained) {
                events.onFocusGained();
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            // Автоповтор при удержании не считаем новым нажатием.
            if (!event.key.repeat) {
                input.keyDown(toKey(event.key.scancode));
            }
            break;
        case SDL_EVENT_KEY_UP: input.keyUp(toKey(event.key.scancode)); break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (const auto button = toMouseButton(event.button.button)) {
                input.mouseButtonDown(*button);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (const auto button = toMouseButton(event.button.button)) {
                input.mouseButtonUp(*button);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION: {
            const Vec2 position(event.motion.x, event.motion.y);
            Vec2 delta(event.motion.xrel, event.motion.yrel);
            // При переносе курсора xrel содержал бы скачок через всё окно.
            if (m_mouseWrapping && !m_warpUnsupported) {
                delta = wrappedMouseDelta(position);
            }
            input.mouseMoved(position, delta);
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            // В некоторых системах направление прокрутки перевёрнуто, приводим к одному виду.
            const float delta = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
            input.wheelScrolled(delta);
            break;
        }
        default: break;
        }
    }
}

void Window::setMouseWrapping(bool wrapping)
{
    m_mouseWrapping = wrapping;
    if (!wrapping) {
        return;
    }
    float x = 0.0f;
    float y = 0.0f;
    SDL_GetMouseState(&x, &y);
    m_mouseWrap.reset({x, y});
}

Vec2 Window::wrappedMouseDelta(const Vec2& position)
{
    const Vec2 size = sizeInPoints();
    const Vec2 delta = m_mouseWrap.delta(position, size);
    if (m_mouseWrap.warpIgnored()) {
        m_warpUnsupported = true;
        log::warn("Mouse warping does not work here, the cursor will not wrap around the window edges");

        return delta;
    }
    if (const auto target = m_mouseWrap.wrapTarget(position, size)) {
        SDL_WarpMouseInWindow(m_window, target->x, target->y);
        m_mouseWrap.warped(*target);
    }

    return delta;
}

void Window::setTitle(const std::string& title)
{
    SDL_SetWindowTitle(m_window, title.c_str());
}

void Window::setIcon(int width, int height, std::span<const std::uint8_t> rgba)
{
    constexpr int kBytesPerPixel = 4;
    // SDL не меняет пиксели поверхности, которую только читает для иконки.
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        width,
        height,
        SDL_PIXELFORMAT_RGBA32,
        const_cast<std::uint8_t*>(rgba.data()),
        width * kBytesPerPixel
    );
    if (!surface) {
        log::warn("Window icon is not set: {}", SDL_GetError());

        return;
    }
    if (!SDL_SetWindowIcon(m_window, surface)) {
        log::warn("Window icon is not set: {}", SDL_GetError());
    }
    SDL_DestroySurface(surface);
}

Vec2 Window::sizeInPoints() const
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(m_window, &w, &h);

    return {static_cast<float>(w), static_cast<float>(h)};
}

int Window::widthInPixels() const
{
    int w = 0;
    SDL_GetWindowSizeInPixels(m_window, &w, nullptr);

    return w;
}

int Window::heightInPixels() const
{
    int h = 0;
    SDL_GetWindowSizeInPixels(m_window, nullptr, &h);

    return h;
}

} // namespace yqlora
