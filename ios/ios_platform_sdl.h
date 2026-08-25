#ifndef ENGINE_SIM_IOS_PLATFORM_SDL_H
#define ENGINE_SIM_IOS_PLATFORM_SDL_H

#include "../include/desktop_platform.h"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class IosPlatformSdl final : public DesktopPlatform
{
public:
    IosPlatformSdl();
    ~IosPlatformSdl() override;

    bool initialize(
        const std::string &title,
        int width,
        int height) override;

    void pumpEvents() override;
    void shutdown() override;

    void handleEvent(
        const SDL_Event &event);

    bool shouldQuit() const override;

    bool isKeyDown(
        DesktopKey key) const override;

    bool wasKeyPressed(
        DesktopKey key) const override;

    bool wasMouseButtonPressed(
        DesktopMouseButton button) const override;

    bool wasMouseButtonReleased(
        DesktopMouseButton button) const override;

    void mousePosition(
        int *x,
        int *y) const override;

    const std::vector<DesktopTouchEvent> &
    touchEvents() const override;

    float mouseWheelY() const override;

    int windowWidth() const override;
    int windowHeight() const override;

    bool isFullscreen() const override;

    void setFullscreen(
        bool enabled) override;

    bool openUrl(
        const std::string &url) override;

    void *nativeWindowHandle() const override;

    std::uint64_t ticks() const override;

    void delay(
        std::uint32_t milliseconds) const override;

    std::string applicationDirectory() const override;

    std::string lastError() const override;

private:
    static std::size_t keyIndex(
        DesktopKey key);

    static std::size_t mouseButtonIndex(
        DesktopMouseButton button);

    void updateWindowSize();

    void updatePointer(
        float x,
        float y);

private:
    SDL_Window *m_window;

    int m_windowWidth;
    int m_windowHeight;

    int m_mouseX;
    int m_mouseY;

    float m_mouseWheelY;

    bool m_shouldQuit;
    bool m_fullscreen;

    std::array<bool, 64> m_keysDown;
    std::array<bool, 64> m_keysPressed;

    std::array<bool, 3> m_mousePressed;
    std::array<bool, 3> m_mouseReleased;

    std::array<bool, 3> m_pendingMousePressed;
    std::array<bool, 3> m_pendingMouseReleased;

    std::vector<DesktopTouchEvent> m_touchEvents;
    std::vector<DesktopTouchEvent> m_pendingTouchEvents;
};

#endif
