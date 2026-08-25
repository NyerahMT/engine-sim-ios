#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "ios_platform_sdl.h"

#include "../include/engine_sim_application.h"
#include "../include/runtime_paths.h"
#include "../include/sdl_audio_output.h"
#include "../include/sdl_gpu_renderer.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

struct EngineSimIOSState
{
    IosPlatformSdl platform;

    SdlGpuRenderer renderer;

    SdlAudioOutput audioOutput;

    EngineSimApplication application;

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
        " Real application host starting\n"
        "========================================\n");

    auto *state =
        new EngineSimIOSState();

    *appstate = state;

    /*
     * Create the SDL-backed native iOS window.
     */

    if (!state->platform.initialize(
        "EngineSim",
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
     * Resolve the EngineSim asset directory.
     *
     * GitHub packaging puts:
     *
     * EngineSim.app/
     * ├── EngineSim
     * └── assets/
     */

    const RuntimePaths paths =
        RuntimePaths::discover(
            state->platform
                .applicationDirectory(),
            {});

    printPathStatus(
        paths);

    /*
     * Initialize EngineSim's actual SDL GPU renderer.
     *
     * On Apple this renderer prefers the MSL shader
     * files and SDL's Metal backend.
     */

    const std::filesystem::path shaderDirectory =
        paths.assetDirectory
        / "shaders";

    if (!state->renderer.initialize(
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
     * This is the moment the real program starts.
     *
     * EngineSimApplication::initialize:
     *
     * - loads the font
     * - loads authored meshes
     * - initializes geometry generation
     * - loads main.mr
     * - creates the engine
     * - creates the simulator
     * - creates the actual UI
     * - attaches audio output
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
        "========================================\n");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(
    void *appstate,
    SDL_Event *event)
{
    auto *state =
        static_cast<EngineSimIOSState *>(
            appstate);

    if (
        state == nullptr
        || event == nullptr)
    {
        return SDL_APP_CONTINUE;
    }

    /*
     * Hand SDL's native iOS touch/window events
     * into the EngineSim platform abstraction.
     */

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
        static_cast<EngineSimIOSState *>(
            appstate);

    if (
        state == nullptr
        || !state->applicationInitialized)
    {
        return SDL_APP_FAILURE;
    }

    /*
     * Drive one frame of REAL EngineSim.
     *
     * This replaces the fake tachometer loop
     * from our previous milestone.
     */

    const bool continueRunning =
        state->application.tick();

    if (!continueRunning)
    {
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
        static_cast<EngineSimIOSState *>(
            appstate);

    if (state == nullptr)
    {
        return;
    }

    std::printf(
        "EngineSim iOS shutting down.\n");

    if (state->applicationInitialized)
    {
        state->application.destroy();

        state->applicationInitialized =
            false;
    }

    if (state->rendererInitialized)
    {
        state->renderer.shutdown();

        state->rendererInitialized =
            false;
    }

    state->platform.shutdown();

    delete state;
}
