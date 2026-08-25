#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "ios_platform_sdl.h"

#include "../include/engine_catalog.h"
#include "../include/engine_sim_application.h"
#include "../include/runtime_paths.h"
#include "../include/sdl_audio_output.h"
#include "../include/sdl_gpu_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

namespace {

namespace fs = std::filesystem;

fs::path customEngineDirectory() {
    const char *home =
        std::getenv("HOME");

    if (
        home == nullptr
        || home[0] == '\0')
    {
        return {};
    }

    return
        fs::path(home)
        / "Documents"
        / "Custom Engines";
}

bool isMrFile(
    const fs::path &path)
{
    std::string extension =
        path.extension()
            .string();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    return extension == ".mr";
}

/*
 * Copy a document handed to us by iOS into the application's permanent
 * custom-engine directory.
 *
 * If the file is already there, simply return the existing path.
 */
fs::path importEngineFile(
    const fs::path &source)
{
    if (
        source.empty()
        || !isMrFile(source))
    {
        return {};
    }

    std::error_code error;

    if (
        !fs::exists(
            source,
            error)
        || error)
    {
        std::fprintf(
            stderr,
            "Custom engine import: source does not exist: %s\n",
            source.string().c_str());

        return {};
    }

    const fs::path customRoot =
        customEngineDirectory();

    if (customRoot.empty()) {
        return {};
    }

    fs::create_directories(
        customRoot,
        error);

    if (error) {
        std::fprintf(
            stderr,
            "Custom engine import: could not create directory: %s\n",
            error.message().c_str());

        return {};
    }

    /*
     * If Files already handed us something inside Custom Engines,
     * don't make another copy.
     */
    std::error_code canonicalError;

    const fs::path canonicalSource =
        fs::weakly_canonical(
            source,
            canonicalError);

    canonicalError.clear();

    const fs::path canonicalRoot =
        fs::weakly_canonical(
            customRoot,
            canonicalError);

    if (
        !canonicalError
        && canonicalSource
            .string()
            .rfind(
                canonicalRoot.string(),
                0)
                == 0)
    {
        return canonicalSource;
    }

    fs::path destination =
        customRoot
        / source.filename();

    /*
     * Don't silently overwrite an existing engine with a different
     * downloaded file.
     */
    if (
        fs::exists(
            destination,
            error)
        && !error)
    {
        const std::string stem =
            destination
                .stem()
                .string();

        const std::string extension =
            destination
                .extension()
                .string();

        int suffix =
            2;

        do {
            destination =
                customRoot
                / (
                    stem
                    + " "
                    + std::to_string(
                        suffix)
                    + extension);

            ++suffix;

            error.clear();
        }
        while (
            fs::exists(
                destination,
                error)
            && !error);
    }

    error.clear();

    fs::copy_file(
        source,
        destination,
        fs::copy_options::overwrite_existing,
        error);

    if (error) {
        std::fprintf(
            stderr,
            "Custom engine import failed: %s\n",
            error.message().c_str());

        return {};
    }

    std::printf(
        "Imported custom engine:\n%s\n",
        destination.string().c_str());

    return destination;
}

}

/*
 * iOS-specific EngineSim application driver.
 *
 * Physics/audio remain tied to real elapsed time while rendering follows
 * the native iOS display-link cadence, allowing ProMotion displays to run
 * up to 120 Hz.
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
            const float uiDt =
                std::min(
                    dt,
                    1.0f / 30.0f);

            m_uiManager.update(
                uiDt);
        }

        /*
         * Selected/downloaded engines are loaded only after the current
         * UI update has completed.
         */
        if (
            !m_pendingScriptPath.empty())
        {
            const std::string selectedScript =
                m_pendingScriptPath;

            m_pendingScriptPath.clear();

            if (
                loadScript(
                    selectedScript))
            {
                m_currentScriptPath =
                    selectedScript;
            }
            else {
                std::fprintf(
                    stderr,
                    "Engine script failed to load:\n%s\n",
                    selectedScript.c_str());
            }
        }

        /*
         * Render once for every iOS display-link callback.
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
     * Ensure Files has a destination ready immediately.
     */
    {
        std::error_code error;

        fs::create_directories(
            customEngineDirectory(),
            error);
    }

    const RuntimePaths paths =
        RuntimePaths::discover(
            state->platform
                .applicationDirectory(),
            {});

    printPathStatus(paths);

    const fs::path shaderDirectory =
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

    state->application.initialize(
        &state->platform,
        &state->renderer,
        &state->audioOutput,
        paths);

    state->applicationInitialized =
        true;

    refreshEngineCatalog();

    std::printf(
        "========================================\n"
        " Real EngineSim application initialized\n"
        " ProMotion presentation enabled\n"
        " Custom engine importing enabled\n"
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

    /*
     * SDL translates an iOS document-open/drop operation into a DROP_FILE
     * event. That gives us a real filesystem path to the document handed
     * over by Files, Safari, Discord, etc.
     */
    if (
        event->type
        == SDL_EVENT_DROP_FILE
        && event->drop.data
        != nullptr)
    {
        const fs::path source(
            event->drop.data);

        std::printf(
            "Received document from iOS:\n%s\n",
            source.string().c_str());

        const fs::path imported =
            importEngineFile(
                source);

        if (!imported.empty()) {
            /*
             * Make it immediately visible in the picker.
             */
            refreshEngineCatalog();

            /*
             * And load it immediately.
             *
             * This uses the same deferred engine-switch path as tapping
             * a picker button, keeping UI destruction safe.
             */
            if (
                state
                    ->applicationInitialized)
            {
                state
                    ->application
                    .requestEngineScript(
                        imported.string());
            }
        }
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
