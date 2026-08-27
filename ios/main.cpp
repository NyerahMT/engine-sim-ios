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
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

/*
 * Implemented by document_open_bridge.mm.
 */
std::vector<std::string>
engineSimTakePendingDocumentPaths();

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

bool filesAreIdentical(
    const fs::path &left,
    const fs::path &right)
{
    std::error_code error;

    const std::uintmax_t leftSize =
        fs::file_size(
            left,
            error);

    if (error) {
        return false;
    }

    error.clear();

    const std::uintmax_t rightSize =
        fs::file_size(
            right,
            error);

    if (
        error
        || leftSize != rightSize)
    {
        return false;
    }

    std::ifstream leftFile(
        left,
        std::ios::in
            | std::ios::binary);

    std::ifstream rightFile(
        right,
        std::ios::in
            | std::ios::binary);

    if (
        !leftFile.is_open()
        || !rightFile.is_open())
    {
        return false;
    }

    constexpr std::size_t BufferSize =
        16 * 1024;

    char leftBuffer[BufferSize];
    char rightBuffer[BufferSize];

    while (leftFile && rightFile) {
        leftFile.read(
            leftBuffer,
            static_cast<std::streamsize>(
                BufferSize));

        rightFile.read(
            rightBuffer,
            static_cast<std::streamsize>(
                BufferSize));

        const std::streamsize leftCount =
            leftFile.gcount();

        const std::streamsize rightCount =
            rightFile.gcount();

        if (
            leftCount != rightCount
            || !std::equal(
                leftBuffer,
                leftBuffer + leftCount,
                rightBuffer))
        {
            return false;
        }
    }

    return true;
}

/*
 * Copy a document handed to us by iOS into the application's permanent
 * custom-engine directory.
 *
 * If the exact same file is already present, reuse it.
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

        std::fflush(stderr);

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

        std::fflush(stderr);

        return {};
    }

    /*
     * If the handed-off document is already physically inside the
     * Custom Engines directory, just use it directly.
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
     * If this exact engine is already saved, reuse it.
     *
     * If something else uses the same filename, preserve both by assigning
     * a suffix rather than overwriting the existing engine.
     */
    if (
        fs::exists(
            destination,
            error)
        && !error)
    {
        if (
            filesAreIdentical(
                source,
                destination))
        {
            std::fprintf(
                stderr,
                "Custom engine import: already saved, reusing: %s\n",
                destination.string().c_str());

            std::fflush(stderr);

            return destination;
        }

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

    std::fprintf(
        stderr,
        "Custom engine import: copying\n"
        "  source: %s\n"
        "  destination: %s\n",
        source.string().c_str(),
        destination.string().c_str());

    std::fflush(stderr);

    fs::copy_file(
        source,
        destination,
        fs::copy_options::overwrite_existing,
        error);

    if (error) {
        std::fprintf(
            stderr,
            "Custom engine import failed during copy: %s\n",
            error.message().c_str());

        std::fflush(stderr);

        return {};
    }

    /*
     * Don't report success until the permanent copy actually exists and
     * contains data.
     */
    error.clear();

    if (
        !fs::exists(
            destination,
            error)
        || error)
    {
        std::fprintf(
            stderr,
            "Custom engine import failed verification: destination missing: %s\n",
            destination.string().c_str());

        std::fflush(stderr);

        return {};
    }

    error.clear();

    const std::uintmax_t savedSize =
        fs::file_size(
            destination,
            error);

    if (
        error
        || savedSize == 0)
    {
        std::fprintf(
            stderr,
            "Custom engine import failed verification: invalid saved file: %s\n",
            destination.string().c_str());

        std::fflush(stderr);

        std::error_code cleanupError;

        fs::remove(
            destination,
            cleanupError);

        return {};
    }

    std::fprintf(
        stderr,
        "Custom engine import SAVED permanently: %s (%ju bytes)\n",
        destination.string().c_str(),
        static_cast<std::uintmax_t>(
            savedSize));

    std::fflush(stderr);

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

    void resetFrameClock()
    {
        if (m_platform == nullptr) {
            m_lastTick = 0;
            m_lastRenderTick = 0;
            return;
        }

        const std::uint64_t now =
            m_platform->ticks();

        m_lastTick =
            now;

        m_lastRenderTick =
            now;
    }

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

                std::fflush(stderr);
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

    std::atomic<bool> suspended{
        false
    };

    std::atomic<bool> terminating{
        false
    };

    std::atomic<bool> resumePending{
        false
    };
};

/*
 * Pull any URLs captured by document_open_bridge.mm into the ordinary
 * Engine Simulator import/load path.
 */
static void processNativeDocumentImports(
    EngineSimIOSState *state)
{
    if (
        state == nullptr
        || !state->applicationInitialized)
    {
        return;
    }

    std::vector<std::string>
        pending =
            engineSimTakePendingDocumentPaths();

    if (pending.empty()) {
        return;
    }

    std::fprintf(
        stderr,
        "[DocumentBridge] EngineSim processing %zu queued document(s)\n",
        pending.size());

    std::fflush(stderr);

    for (
        const std::string &path
        : pending)
    {
        std::fprintf(
            stderr,
            "[DocumentBridge] Import BEGIN: %s\n",
            path.c_str());

        std::fflush(stderr);

        const fs::path imported =
            importEngineFile(
                fs::path(path));

        if (imported.empty()) {
            std::fprintf(
                stderr,
                "[DocumentBridge] Import FAILED: %s\n",
                path.c_str());

            std::fflush(stderr);

            continue;
        }

        std::fprintf(
            stderr,
            "[DocumentBridge] Import SUCCESS: %s\n",
            imported.string().c_str());

        std::fflush(stderr);

        /*
         * Make the newly imported engine immediately visible in the custom
         * engine picker.
         */
        refreshEngineCatalog();

        /*
         * Use the same safe deferred engine-switch path as picker buttons.
         */
        state
            ->application
            .requestEngineScript(
                imported.string());
    }
}

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

    printPathStatus(
        paths);

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

    /*
     * If this launch happened because the user opened an .mr file, UIKit
     * may already have staged it before SDL_AppInit ran.
     */
    processNativeDocumentImports(
        state);

    std::printf(
        "========================================\n"
        " Real EngineSim application initialized\n"
        " ProMotion presentation enabled\n"
        " Custom engine importing enabled\n"
        " Native iOS document bridge enabled\n"
        " iOS lifecycle handling enabled\n"
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

    switch (event->type) {
        case SDL_EVENT_TERMINATING:
        {
            std::printf(
                "iOS lifecycle: terminating.\n");

            state->terminating.store(
                true,
                std::memory_order_release);

            state->suspended.store(
                true,
                std::memory_order_release);

            return SDL_APP_SUCCESS;
        }

        case SDL_EVENT_WILL_ENTER_BACKGROUND:
        {
            std::printf(
                "iOS lifecycle: will enter background.\n");

            state->suspended.store(
                true,
                std::memory_order_release);

            break;
        }

        case SDL_EVENT_DID_ENTER_BACKGROUND:
        {
            std::printf(
                "iOS lifecycle: entered background.\n");

            state->suspended.store(
                true,
                std::memory_order_release);

            break;
        }

        case SDL_EVENT_WILL_ENTER_FOREGROUND:
        {
            std::printf(
                "iOS lifecycle: will enter foreground.\n");

            break;
        }

        case SDL_EVENT_DID_ENTER_FOREGROUND:
        {
            std::printf(
                "iOS lifecycle: entered foreground.\n");

            state->resumePending.store(
                true,
                std::memory_order_release);

            state->suspended.store(
                false,
                std::memory_order_release);

            break;
        }

        case SDL_EVENT_LOW_MEMORY:
        {
            std::fprintf(
                stderr,
                "iOS lifecycle: low-memory warning.\n");

            break;
        }

        default:
            break;
    }

    /*
     * Keep SDL's normal DROP_FILE path as a fallback.
     *
     * The native UIKit bridge handles the unreliable cold-start case, but
     * if SDL successfully delivers a file itself we still accept it.
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
            "Received document from SDL:\n%s\n",
            source.string().c_str());

        const fs::path imported =
            importEngineFile(
                source);

        if (!imported.empty()) {
            refreshEngineCatalog();

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

    if (
        !state->suspended.load(
            std::memory_order_acquire)
        && !state->terminating.load(
            std::memory_order_acquire))
    {
        state->platform.handleEvent(
            *event);
    }

    if (
        event->type
        == SDL_EVENT_QUIT)
    {
        state->terminating.store(
            true,
            std::memory_order_release);

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

    if (
        state->terminating.load(
            std::memory_order_acquire))
    {
        return SDL_APP_SUCCESS;
    }

    /*
     * Drain native document URLs before checking suspension.
     *
     * This lets a file opened while the app is backgrounded get imported
     * immediately and queued for loading as soon as we foreground again.
     */
    processNativeDocumentImports(
        state);

    if (
        state->suspended.load(
            std::memory_order_acquire))
    {
        return SDL_APP_CONTINUE;
    }

    if (
        state->resumePending.exchange(
            false,
            std::memory_order_acq_rel))
    {
        state->application
            .resetFrameClock();
    }

    const bool continueRunning =
        state
            ->application
            .tickProMotion();

    if (!continueRunning) {
        state->terminating.store(
            true,
            std::memory_order_release);

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

    state->terminating.store(
        true,
        std::memory_order_release);

    state->suspended.store(
        true,
        std::memory_order_release);

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
