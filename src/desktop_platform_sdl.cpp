#include "../include/desktop_platform_sdl.h"

#include <SDL3/SDL.h>

#include <filesystem>

DesktopPlatformSdl::DesktopPlatformSdl()
    : m_window(nullptr), m_keysDown{}, m_keysPressed{}, m_mousePressed{},
      m_mouseReleased{}, m_shouldQuit(false), m_fullscreen(false),
      m_windowWidth(0), m_windowHeight(0), m_mouseX(0), m_mouseY(0),
      m_mouseWheelY(0.0f) { }

DesktopPlatformSdl::~DesktopPlatformSdl() {
    shutdown();
}

bool DesktopPlatformSdl::initialize(const std::string &title, int width, int height) {
    // SDL3 keeps audio as a separate subsystem. Opening a device stream
    // without this initialization can fail silently on desktop platforms,
    // leaving the simulator's PCM producer disconnected from playback.
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) return false;

    m_window = SDL_CreateWindow(title.c_str(), width, height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (m_window == nullptr) {
        SDL_Quit();
        return false;
    }

    SDL_GetWindowSizeInPixels(m_window, &m_windowWidth, &m_windowHeight);
    return true;
}

void DesktopPlatformSdl::pumpEvents() {
    m_keysPressed.fill(false);
    m_mousePressed.fill(false);
    m_mouseReleased.fill(false);
    m_mouseWheelY = 0.0f;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            m_shouldQuit = true;
            break;
        case SDL_EVENT_KEY_DOWN: {
            const int scancode = event.key.scancode;
            for (std::size_t i = 0; i < KeyCount; ++i) {
                if (sdlScancode(static_cast<DesktopKey>(i)) == scancode) {
                    m_keysPressed[i] = !event.key.repeat && !m_keysDown[i];
                    m_keysDown[i] = true;
                    break;
                }
            }
            break;
        }
        case SDL_EVENT_KEY_UP: {
            const int scancode = event.key.scancode;
            for (std::size_t i = 0; i < KeyCount; ++i) {
                if (sdlScancode(static_cast<DesktopKey>(i)) == scancode) {
                    m_keysDown[i] = false;
                    break;
                }
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            m_mouseX = static_cast<int>(event.motion.x);
            m_mouseY = static_cast<int>(event.motion.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            DesktopMouseButton button;
            if (event.button.button == SDL_BUTTON_LEFT) button = DesktopMouseButton::Left;
            else if (event.button.button == SDL_BUTTON_MIDDLE) button = DesktopMouseButton::Middle;
            else if (event.button.button == SDL_BUTTON_RIGHT) button = DesktopMouseButton::Right;
            else break;
            const std::size_t index = mouseButtonIndex(button);
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) m_mousePressed[index] = true;
            else m_mouseReleased[index] = true;
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            m_mouseWheelY += event.wheel.y;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            m_windowWidth = event.window.data1;
            m_windowHeight = event.window.data2;
            break;
        default:
            break;
        }
    }
}

void DesktopPlatformSdl::shutdown() {
    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

bool DesktopPlatformSdl::isKeyDown(DesktopKey key) const { return m_keysDown[keyIndex(key)]; }
bool DesktopPlatformSdl::wasKeyPressed(DesktopKey key) const { return m_keysPressed[keyIndex(key)]; }
bool DesktopPlatformSdl::wasMouseButtonPressed(DesktopMouseButton button) const { return m_mousePressed[mouseButtonIndex(button)]; }
bool DesktopPlatformSdl::wasMouseButtonReleased(DesktopMouseButton button) const { return m_mouseReleased[mouseButtonIndex(button)]; }
void DesktopPlatformSdl::mousePosition(int *x, int *y) const { *x = m_mouseX; *y = m_mouseY; }
float DesktopPlatformSdl::mouseWheelY() const { return m_mouseWheelY; }

void DesktopPlatformSdl::setFullscreen(bool enabled) {
    if (m_window == nullptr || !SDL_SetWindowFullscreen(m_window, enabled)) return;
    m_fullscreen = enabled;
}

std::uint64_t DesktopPlatformSdl::ticks() const { return SDL_GetTicks(); }
void DesktopPlatformSdl::delay(std::uint32_t milliseconds) const { SDL_Delay(milliseconds); }

std::string DesktopPlatformSdl::applicationDirectory() const {
    const char *basePath = SDL_GetBasePath();
    if (basePath == nullptr) return {};
    return basePath;
}

std::string DesktopPlatformSdl::lastError() const { return SDL_GetError(); }
std::size_t DesktopPlatformSdl::keyIndex(DesktopKey key) { return static_cast<std::size_t>(key); }
std::size_t DesktopPlatformSdl::mouseButtonIndex(DesktopMouseButton button) { return static_cast<std::size_t>(button); }

int DesktopPlatformSdl::sdlScancode(DesktopKey key) {
    static constexpr int scancodes[] = {
        SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RETURN, SDL_SCANCODE_TAB, SDL_SCANCODE_SPACE,
        SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
        SDL_SCANCODE_INSERT, SDL_SCANCODE_COMMA,
        SDL_SCANCODE_A, SDL_SCANCODE_B, SDL_SCANCODE_C, SDL_SCANCODE_D, SDL_SCANCODE_E,
        SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_I, SDL_SCANCODE_M,
        SDL_SCANCODE_N, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T,
        SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_SCANCODE_X, SDL_SCANCODE_Y,
        SDL_SCANCODE_Z, SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4,
        SDL_SCANCODE_F5, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
        SDL_SCANCODE_5
    };
    return scancodes[keyIndex(key)];
}
