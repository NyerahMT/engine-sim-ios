#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "ios_platform_sdl.h"

#include "../include/engine_sim_application.h"
#include "../include/runtime_paths.h"
#include "../include/sdl_audio_output.h"
#include "../include/sdl_gpu_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

/*
 * iOS-specific EngineSim application driver.
 *
 * Upstream EngineSimApplication::tick() deliberately limits the iOS
 * presentation rate to 60 FPS while idle and ~30 FPS while running.
 *
 * That made sense while we were fighting the original performance/audio
 * problems, but those bottlenecks are gone now.
 *
 * This driver keeps:
 *
 *   - physics tied to actual elapsed time
 *   - audio timing unchanged
 *   - simulation frequency unchanged
 *
 * while allowing presentation on every iOS display-link callback.
 *
 * On a ProMotion device that means up to 120 FPS.
 * On a 60 Hz device it naturally remains 60 FPS.
 */
class EngineSimIOSApplication final
    : public EngineSimApplication
{
public:
    bool tickProMotion()
    {
        if (m_platform == nullptr) {
            return false;
        }

        if (m_lastTick == 0) {
            m_lastTick =
                m_platform->ticks();
        }

        /*
         * SDL's iOS callback lifecycle is display-link driven.
         *
         * Do not impose another software FPS limiter here.
         */
        m_platform->pumpEvents();

        if (
            m_platform->shouldQuit()
            || m_platform->wasKeyPressed(
                DesktopKey::Escape))
        {
            return false;
        }

        const std::uint64_t now =
            m_platform->ticks();

        /*
         * Physics remains based on REAL elapsed time.
         *
         * At 120 Hz this will normally be about 8.3 ms.
         * At 60 Hz it will normally be about 16.7 ms.
         *
         * Therefore 120 FPS does NOT double the amount of simulated
         * engine time. It simply divides the same real-time simulation
         * workload into smaller chunks.
         */
        const float dt =
            std::min(
                static_cast<float>(
                    now - m_lastTick)
                    / 1000.0f,
                0.25f);

        m_lastTick =
            now;

        if (dt > 0.0f) {
            m_averageFramerate =
                0.9f
                    * m_averageFramerate
                + 0.1f
                    / dt;
        }

        if (
            m_platform->wasKeyPressed(
                DesktopKey::F))
        {
            toggleFullscreen();
        }

        if (
            m_platform->wasKeyPressed(
                DesktopKey::Tab))
        {
            m_screen =
                (m_screen + 1)
                % 3;
        }

        if (
            m_platform->wasKeyPressed(
                DesktopKey::Return))
        {
            loadScript(
                m_currentScriptPath);
        }

        m_screenWidth =
            m_platform->windowWidth();

        m_screenHeight =
            m_platform->windowHeight();

        if (dt > 0.0f) {
            processEngineInput(dt);

            if (
                !m_paused
                || m_platform->wasKeyPressed(
                    DesktopKey::Right))
            {
                process(dt);
            }
        }

        if (
            m_engineView
            != nullptr)
        {
            /*
             * At ProMotion rates the gauge springs now receive smaller,
             * more frequent time steps. Keep the old hitch protection,
             * but do not artificially reduce their update rate.
             */
            const float uiDt =
                std::min(
                    dt,
                    1.0f / 30.0f);

            m_uiManager.update(
                uiDt);
        }

        /*
         * Engine selection is intentionally deferred until after the UI
         * update finishes so the picker cannot destroy itself while its
         * button event is still being dispatched.
         */
        if (
            !m_pendingScriptPath.empty())
        {
            const std::string
                selectedScript =
                    m_pendingScriptPath;

            m_pendingScriptPath.clear();

            if (
                loadScript(
                    selectedScript))
            {
                m_currentScriptPath =
                    selectedScript;
            }
        }

        /*
         * THE PROMOTION CHANGE:
         *
         * Render once for every SDL iOS display-link iteration.
         *
         * There is deliberately no 8 ms timer here. The display itself is
         * the clock, which gives us clean pacing at 120/90/80/60 Hz as iOS
         * dynamically changes the ProMotion refresh rate.
         */
        renderScene();

        m_lastRenderTick =
            now;

        return true;
    }
};

struct EngineSimIOSState
{
    IosPlatformSdl platform;

    SdlGpuRenderer renderer;

    SdlAudioOutput audioOutput;

    EngineSimIOSApplication application;

    bool applicationInitialized =
        false;

    bool rendererInitialized =
        false;
};

static void printPathStatus(
    const RuntimePaths &paths)
{
    std::printf(
        "========================================\n");

    std::printf(
        "EngineSim iOS runtime paths\n");

    std::printf(
        "========================================\n");

    std::printf(
        "Application directory:\n%s\n",
        paths.applicationDirectory
            .string()
            .c_str());

    std::printf(
        "\nAsset directory:\n%s\n",
        paths.assetDirectory
            .string()
            .c_str());

    std::printf(
        "\nmain.mr: %s\n",
        std::filesystem::exists(
            paths.assetDirectory
            / "main.mr")
            ? "FOUND"
            : "MISSING");

    std::printf(
        "font: %s\n",
        std::filesystem::exists(
            paths.assetDirectory
            / "fonts/slkscr.ttf")
            ? "FOUND"
            : "MISSING");

    std::printf(
        "mesh library: %s\n",
        std::filesystem::exists(
            paths.assetDirectory
            / "authored_meshes.obj")
            ? "FOUND"
            : "MISSING");

    std::printf(
        "Metal vertex shader: %s\n",
        std::filesystem::exists(
            paths.assetDirectory
            / "shaders/engine_sim.vertex.msl")
            ? "FOUND"
            : "MISSING");

    std::printf(
        "Metal fragment shader: %s\n",
        std::filesystem::exists(
            paths.assetDirectory
            / "shaders/engine_sim.fragment.msl")
            ? "FOUND"
            : "MISSING");

    std::printf(
        "========================================\n");
}

SDL_AppResult SDL_AppInit(
    void **appstate,
    int argc,
    char *argv[])
{
    (void)argc;
    (void)argv;

    std::printf(
        "\n\n"
        "========================================\n"
        " ENGINE SIMULATOR iOS\n"
        " ProMotion application host starting\n"
        "========================================\n");

    auto *state =
        new EngineSimIOSState();

    *appstate =
        state;

    /*
     * Create the SDL-backed native iOS window.
     */
    if (
        !state->platform.initialize(
            "Engine Simulator",
            1920,
            1080))
    {
        std::fprintf(
            stderr,
            "iOS SDL platform initialization failed:\n%s\n",
            state->platform
                .lastError()
                .c_str());

        return SDL_APP_FAILURE;
    }

    /*
     * Resolve EngineSim runtime assets.
     */
    const RuntimePaths paths =
        RuntimePaths::discover(
            state->platform
                .applicationDirectory(),
            {});

    printPathStatus(
        paths);

    /*
     * Initialize the real SDL GPU/Metal renderer.
     */
    const std::filesystem::path
        shaderDirectory =
            paths.assetDirectory
            / "shaders";

    if (
        !state->renderer.initialize(
            state->platform
                .nativeWindowHandle(),
            shaderDirectory.string()))
    {
        std::fprintf(
            stderr,
            "EngineSim GPU renderer initialization failed:\n%s\n",
            state->renderer
                .lastError());

        return SDL_APP_FAILURE;
    }

    state->rendererInitialized =
        true;

    std::printf(
        "EngineSim SDL GPU renderer initialized.\n");

    /*
     * Initialize the actual Engine Simulator application.
     */
    state->application.initialize(
        &state->platform,
        &state->renderer,
        &state->audioOutput,
        paths);

    state->applicationInitialized =
        true;

    std::printf(
        "========================================\n"
        " Real EngineSim application initialized\n"
        " ProMotion presentation enabled\n"
        "========================================\n");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(
    void *appstate,
    SDL_Event *event)
{
    auto *state =
        static_cast<
            EngineSimIOSState *>(
                appstate);

    if (
        state == nullptr
        || event == nullptr)
    {
        return SDL_APP_CONTINUE;
    }

    state->platform.handleEvent(
        *event);

    if (
        event->type
        == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(
    void *appstate)
{
    auto *state =
        static_cast<
            EngineSimIOSState *>(
                appstate);

    if (
        state == nullptr
        || !state
            ->applicationInitialized)
    {
        return SDL_APP_FAILURE;
    }

    /*
     * SDL invokes this from its native iOS display-link callback.
     *
     * tickProMotion() renders exactly once per callback.
     */
    const bool continueRunning =
        state
            ->application
            .tickProMotion();

    if (!continueRunning) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(
    void *appstate,
    SDL_AppResult result)
{
    (void)result;

    auto *state =
        static_cast<
            EngineSimIOSState *>(
                appstate);

    if (state == nullptr) {
        return;
    }

    std::printf(
        "EngineSim iOS shutting down.\n");

    if (
        state
            ->applicationInitialized)
    {
        state
            ->application
            .destroy();

        state
            ->applicationInitialized =
                false;
    }

    if (
        state
            ->rendererInitialized)
    {
        state
            ->renderer
            .shutdown();

        state
            ->rendererInitialized =
                false;
    }

    state
        ->platform
        .shutdown();

    delete state;
}
