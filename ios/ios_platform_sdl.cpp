#include "ios_platform_sdl.h"

#include <algorithm>
#include <utility>

IosPlatformSdl::IosPlatformSdl()
    : m_window(nullptr),
      m_windowWidth(0),
      m_windowHeight(0),
      m_mouseX(0),
      m_mouseY(0),
      m_mouseWheelY(0.0f),
      m_shouldQuit(false),
      m_fullscreen(true),
      m_keysDown{},
      m_keysPressed{},
      m_mousePressed{},
      m_mouseReleased{},
      m_pendingMousePressed{},
      m_pendingMouseReleased{}
{
}

IosPlatformSdl::~IosPlatformSdl()
{
    shutdown();
}

bool IosPlatformSdl::initialize(
    const std::string &title,
    int width,
    int height)
{
    if (!SDL_Init(
        SDL_INIT_VIDEO
        | SDL_INIT_AUDIO
        | SDL_INIT_EVENTS))
    {
        return false;
    }

    SDL_WindowFlags flags =
        SDL_WINDOW_HIGH_PIXEL_DENSITY
        | SDL_WINDOW_RESIZABLE;

    m_window =
        SDL_CreateWindow(
            title.c_str(),
            width,
            height,
            flags);

    if (m_window == nullptr)
    {
        SDL_Quit();
        return false;
    }

    updateWindowSize();

    m_shouldQuit = false;
    m_fullscreen = true;

    return true;
}

void IosPlatformSdl::pumpEvents()
{
    /*
     * SDL's callback lifecycle delivers events through
     * SDL_AppEvent before EngineSim's frame tick.
     *
     * Move those staged events into EngineSim's
     * frame-local input state here.
     */

    m_keysPressed.fill(false);

    m_mousePressed =
        m_pendingMousePressed;

    m_mouseReleased =
        m_pendingMouseReleased;

    m_pendingMousePressed.fill(false);
    m_pendingMouseReleased.fill(false);

    m_touchEvents =
        std::move(m_pendingTouchEvents);

    m_pendingTouchEvents.clear();

    m_mouseWheelY = 0.0f;

    updateWindowSize();
}

void IosPlatformSdl::handleEvent(
    const SDL_Event &event)
{
    switch (event.type)
    {
        case SDL_EVENT_QUIT:
        {
            m_shouldQuit = true;
            break;
        }

        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
            updateWindowSize();
            break;
        }

        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
        {
            updateWindowSize();

            const int x =
                static_cast<int>(
                    event.tfinger.x
                    * static_cast<float>(
                        m_windowWidth));

            const int y =
                m_windowHeight
                - static_cast<int>(
                    event.tfinger.y
                    * static_cast<float>(
                        m_windowHeight));

            DesktopTouchEvent::Type type =
                DesktopTouchEvent::Type::Motion;

            if (
                event.type
                == SDL_EVENT_FINGER_DOWN)
            {
                type =
                    DesktopTouchEvent::Type::Down;
            }
            else if (
                event.type
                == SDL_EVENT_FINGER_UP)
            {
                type =
                    DesktopTouchEvent::Type::Up;
            }
            else if (
                event.type
                == SDL_EVENT_FINGER_CANCELED)
            {
                type =
                    DesktopTouchEvent::Type::Canceled;
            }

            m_pendingTouchEvents.push_back(
            {
                static_cast<std::uint64_t>(
                    event.tfinger.fingerID),
                x,
                y,
                type
            });

            m_mouseX = x;
            m_mouseY = y;

            break;
        }

        case SDL_EVENT_MOUSE_MOTION:
        {
            updatePointer(
                event.motion.x,
                event.motion.y);

            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            updatePointer(
                event.button.x,
                event.button.y);

            DesktopMouseButton button;

            if (
                event.button.button
                == SDL_BUTTON_LEFT)
            {
                button =
                    DesktopMouseButton::Left;
            }
            else if (
                event.button.button
                == SDL_BUTTON_MIDDLE)
            {
                button =
                    DesktopMouseButton::Middle;
            }
            else if (
                event.button.button
                == SDL_BUTTON_RIGHT)
            {
                button =
                    DesktopMouseButton::Right;
            }
            else
            {
                break;
            }

            const std::size_t index =
                mouseButtonIndex(button);

            if (
                event.type
                == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                m_pendingMousePressed[index] =
                    true;
            }
            else
            {
                m_pendingMouseReleased[index] =
                    true;
            }

            break;
        }

        case SDL_EVENT_MOUSE_WHEEL:
        {
            m_mouseWheelY +=
                event.wheel.y;

            break;
        }

        default:
        {
            break;
        }
    }
}

void IosPlatformSdl::shutdown()
{
    if (m_window != nullptr)
    {
        SDL_DestroyWindow(
            m_window);

        m_window = nullptr;
    }

    SDL_Quit();
}

bool IosPlatformSdl::shouldQuit() const
{
    return m_shouldQuit;
}

bool IosPlatformSdl::isKeyDown(
    DesktopKey key) const
{
    return m_keysDown[
        keyIndex(key)];
}

bool IosPlatformSdl::wasKeyPressed(
    DesktopKey key) const
{
    return m_keysPressed[
        keyIndex(key)];
}

bool IosPlatformSdl::wasMouseButtonPressed(
    DesktopMouseButton button) const
{
    return m_mousePressed[
        mouseButtonIndex(button)];
}

bool IosPlatformSdl::wasMouseButtonReleased(
    DesktopMouseButton button) const
{
    return m_mouseReleased[
        mouseButtonIndex(button)];
}

void IosPlatformSdl::mousePosition(
    int *x,
    int *y) const
{
    if (x != nullptr)
    {
        *x = m_mouseX;
    }

    if (y != nullptr)
    {
        *y = m_mouseY;
    }
}

const std::vector<DesktopTouchEvent> &
IosPlatformSdl::touchEvents() const
{
    return m_touchEvents;
}

float IosPlatformSdl::mouseWheelY() const
{
    return m_mouseWheelY;
}

int IosPlatformSdl::windowWidth() const
{
    return m_windowWidth;
}

int IosPlatformSdl::windowHeight() const
{
    return m_windowHeight;
}

bool IosPlatformSdl::isFullscreen() const
{
    return m_fullscreen;
}

void IosPlatformSdl::setFullscreen(
    bool enabled)
{
    /*
     * Native iOS application windows already occupy
     * the full application scene.
     */

    m_fullscreen = enabled;
}

bool IosPlatformSdl::openUrl(
    const std::string &url)
{
    return SDL_OpenURL(
        url.c_str());
}

void *IosPlatformSdl::nativeWindowHandle() const
{
    return m_window;
}

std::uint64_t IosPlatformSdl::ticks() const
{
    return SDL_GetTicks();
}

void IosPlatformSdl::delay(
    std::uint32_t milliseconds) const
{
    SDL_Delay(
        milliseconds);
}

std::string
IosPlatformSdl::applicationDirectory() const
{
    const char *path =
        SDL_GetBasePath();

    if (path == nullptr)
    {
        return {};
    }

    return path;
}

std::string
IosPlatformSdl::lastError() const
{
    return SDL_GetError();
}

std::size_t
IosPlatformSdl::keyIndex(
    DesktopKey key)
{
    return static_cast<std::size_t>(
        key);
}

std::size_t
IosPlatformSdl::mouseButtonIndex(
    DesktopMouseButton button)
{
    return static_cast<std::size_t>(
        button);
}

void IosPlatformSdl::updateWindowSize()
{
    if (m_window == nullptr)
    {
        return;
    }

    SDL_GetWindowSizeInPixels(
        m_window,
        &m_windowWidth,
        &m_windowHeight);

    m_windowWidth =
        std::max(
            1,
            m_windowWidth);

    m_windowHeight =
        std::max(
            1,
            m_windowHeight);
}

void IosPlatformSdl::updatePointer(
    float x,
    float y)
{
    int logicalWidth = 0;
    int logicalHeight = 0;

    SDL_GetWindowSize(
        m_window,
        &logicalWidth,
        &logicalHeight);

    updateWindowSize();

    const float scaleX =
        logicalWidth > 0
        ? static_cast<float>(
            m_windowWidth)
            / static_cast<float>(
                logicalWidth)
        : 1.0f;

    const float scaleY =
        logicalHeight > 0
        ? static_cast<float>(
            m_windowHeight)
            / static_cast<float>(
                logicalHeight)
        : 1.0f;

    m_mouseX =
        static_cast<int>(
            x * scaleX);

    m_mouseY =
        m_windowHeight
        - static_cast<int>(
            y * scaleY);
}
