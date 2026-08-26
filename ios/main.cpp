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
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

namespace {

namespace fs = std::filesystem;

/*
 * ============================================================
 * Persistent diagnostic logger
 * ============================================================
 *
 * Written to:
 *
 *     Documents/engine-sim.log
 *
 * Every line is flushed immediately so the log survives a crash,
 * watchdog termination, or force-close as well as possible.
 */

FILE *g_logFile =
    nullptr;

std::uint64_t g_logStartMs =
    0;

std::uint64_t monotonicMilliseconds()
{
    using namespace std::chrono;

    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(
            steady_clock::now()
                .time_since_epoch())
            .count());
}

fs::path documentsDirectory()
{
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
        / "Documents";
}

fs::path diagnosticLogPath()
{
    const fs::path documents =
        documentsDirectory();

    if (documents.empty()) {
        return {};
    }

    return
        documents
        / "engine-sim.log";
}

void diagnosticLog(
    const char *format,
    ...)
{
    if (g_logFile == nullptr) {
        return;
    }

    const std::uint64_t now =
        monotonicMilliseconds();

    const std::uint64_t elapsed =
        now >= g_logStartMs
            ? now - g_logStartMs
            : 0;

    std::fprintf(
        g_logFile,
        "[%8llu ms] ",
        static_cast<unsigned long long>(
            elapsed));

    va_list args;

    va_start(
        args,
        format);

    std::vfprintf(
        g_logFile,
        format,
        args);

    va_end(args);

    std::fprintf(
        g_logFile,
        "\n");

    /*
     * Critical for a diagnostic build:
     * never leave important startup information sitting in stdio buffers.
     */
    std::fflush(
        g_logFile);
}

void openDiagnosticLog()
{
    g_logStartMs =
        monotonicMilliseconds();

    const fs::path path =
        diagnosticLogPath();

    if (path.empty()) {
        return;
    }

    std::error_code error;

    fs::create_directories(
        path.parent_path(),
        error);

    /*
     * Append instead of replacing.
     *
     * That means repeated failed launches remain visible in one file,
     * separated by SESSION START banners.
     */
    g_logFile =
        std::fopen(
            path.string().c_str(),
            "a");

    if (g_logFile == nullptr) {
        return;
    }

    std::setvbuf(
        g_logFile,
        nullptr,
        _IONBF,
        0);

    std::fprintf(
        g_logFile,
        "\n\n"
        "============================================================\n"
        " ENGINE SIMULATOR iOS DIAGNOSTIC SESSION START\n"
        "============================================================\n");

    std::fflush(
        g_logFile);

    diagnosticLog(
        "Persistent logger opened.");

    diagnosticLog(
        "Log path: %s",
        path.string().c_str());
}

void closeDiagnosticLog()
{
    if (g_logFile == nullptr) {
        return;
    }

    diagnosticLog(
        "Diagnostic session closing.");

    std::fprintf(
        g_logFile,
        "============================================================\n"
        " ENGINE SIMULATOR iOS DIAGNOSTIC SESSION END\n"
        "============================================================\n");

    std::fflush(
        g_logFile);

    std::fclose(
        g_logFile);

    g_logFile =
        nullptr;
}

fs::path customEngineDirectory()
{
    const fs::path documents =
        documentsDirectory();

    if (documents.empty()) {
        return {};
    }

    return
        documents
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
 */
fs::path importEngineFile(
    const fs::path &source)
{
    diagnosticLog(
        "importEngineFile entered: %s",
        source.string().c_str());

    if (
        source.empty()
        || !isMrFile(source))
    {
        diagnosticLog(
            "Import rejected: empty path or non-.mr file.");

        return {};
    }

    std::error_code error;

    if (
        !fs::exists(
            source,
            error)
        || error)
    {
        diagnosticLog(
            "Import source missing. error=%s",
            error.message().c_str());

        std::fprintf(
            stderr,
            "Custom engine import: source does not exist: %s\n",
            source.string().c_str());

        return {};
    }

    const fs::path customRoot =
        customEngineDirectory();

    if (customRoot.empty()) {
        diagnosticLog(
            "Import failed: custom engine directory unavailable.");

        return {};
    }

    fs::create_directories(
        customRoot,
        error);

    if (error) {
        diagnosticLog(
            "Import directory creation failed: %s",
            error.message().c_str());

        std::fprintf(
            stderr,
            "Custom engine import: could not create directory: %s\n",
            error.message().c_str());

        return {};
    }

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
        diagnosticLog(
            "Import source already inside Custom Engines.");

        return canonicalSource;
    }

    fs::path destination =
        customRoot
        / source.filename();

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
        diagnosticLog(
            "Import copy failed: %s",
            error.message().c_str());

        std::fprintf(
            stderr,
            "Custom engine import failed: %s\n",
            error.message().c_str());

        return {};
    }

    diagnosticLog(
        "Engine import succeeded: %s",
        destination.string().c_str());

    std::printf(
        "Imported custom engine:\n%s\n",
        destination.string().c_str());

    return destination;
}

}

/*
 * ============================================================
 * iOS EngineSim application
 * ============================================================
 */

class EngineSimIOSApplication final
    : public EngineSimApplication
{
public:

    void resetFrameClock()
    {
        diagnosticLog(
            "resetFrameClock entered.");

        if (m_platform == nullptr) {
            diagnosticLog(
                "resetFrameClock: platform is NULL.");

            m_lastTick =
                0;

            m_lastRenderTick =
                0;

            return;
        }

        const std::uint64_t now =
            m_platform->ticks();

        m_lastTick =
            now;

        m_lastRenderTick =
            now;

        diagnosticLog(
            "resetFrameClock complete. ticks=%llu",
            static_cast<unsigned long long>(
                now));
    }

    bool tickProMotion()
    {
        ++m_diagnosticFrameNumber;

        const bool verboseFrame =
            m_diagnosticFrameNumber <= 20
            || (
                m_diagnosticFrameNumber
                % 120
                == 0);

        if (verboseFrame) {
            diagnosticLog(
                "FRAME %llu: tickProMotion ENTER",
                static_cast<unsigned long long>(
                    m_diagnosticFrameNumber));
        }

        if (m_platform == nullptr) {
            diagnosticLog(
                "FRAME %llu: platform NULL.",
                static_cast<unsigned long long>(
                    m_diagnosticFrameNumber));

            return false;
        }

        if (m_lastTick == 0) {
            m_lastTick =
                m_platform->ticks();

            if (verboseFrame) {
                diagnosticLog(
                    "FRAME %llu: initialized m_lastTick=%llu",
                    static_cast<unsigned long long>(
                        m_diagnosticFrameNumber),
                    static_cast<unsigned long long>(
                        m_lastTick));
            }
        }

        if (verboseFrame) {
            diagnosticLog(
                "FRAME %llu: pumpEvents BEGIN",
                static_cast<unsigned long long>(
                    m_diagnosticFrameNumber));
        }

        m_platform->pumpEvents();

        if (verboseFrame) {
            diagnosticLog(
                "FRAME %llu: pumpEvents END",
                static_cast<unsigned long long>(
                    m_diagnosticFrameNumber));
        }

        if (
            m_platform->shouldQuit()
            || m_platform->wasKeyPressed(
                DesktopKey::Escape))
        {
            diagnosticLog(
                "FRAME %llu: quit requested.",
                static_cast<unsigned long long>(
                    m_diagnosticFrameNumber));

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
            diagnosticLog(
                "Fullscreen toggle requested.");

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
            diagnosticLog(
                "Reload script requested: %s",
                m_currentScriptPath.c_str());

            loadScript(
                m_currentScriptPath);
        }

        m_screenWidth =
            m_platform->windowWidth();

        m_screenHeight =
            m_platform->windowHeight();

        if (verboseFrame) {
            diagnosticLog(
                "FRAME %llu: dt=%.6f, window=%dx%d, avgFPS=%.2f",
                static_cast<unsigned long long>(
                    m_diagnosticFrameNumber),
                static_cast<double>(
                    dt),
                m_screenWidth,
                m_screenHeight,
                static_cast<double>(
                    m_averageFramerate));
        }

        if (dt > 0.0f) {
            if (verboseFrame) {
                diagnosticLog(
                    "FRAME %llu: processEngineInput BEGIN",
                    static_cast<unsigned long long>(
                        m_diagnosticFrameNumber));
            }

            processEngineInput(
                dt);

            if (verboseFrame) {
                diagnosticLog(
                    "FRAME %llu: processEngineInput END",
                    static_cast<unsigned long long>(
                        m_diagnosticFrameNumber));
            }

            if (
                !m_paused
                || m_platform->wasKeyPressed(
                    DesktopKey::Right))
            {
                if (verboseFrame) {
                    diagnosticLog(
                        "FRAME %llu: simulation process BEGIN",
                        static_cast<unsigned long long>(
                            m_diagnosticFrameNumber));
                }

                process(
                    dt);

                if (verboseFrame) {
                    diagnosticLog(
                        "FRAME %llu: simulation process END",
                        static_cast<unsigned long long>(
                            m_diagnosticFrameNumber));
                }
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

            if (verboseFrame) {
                diagnosticLog(
                    "FRAME %llu: UI update BEGIN",
                    static_cast<unsigned long long>(
                        m_diagnosticFrameNumber));
            }

            m_uiManager.update(
                uiDt);

            if (verboseFrame) {
                diagnosticLog(
                    "FRAME %llu: UI update END",
                    static_cast<unsigned long long>(
                        m_diagnosticFrameNumber));
            }
        }
        else if (verboseFrame) {
            diagnosticLog(
                "FRAME %llu: m_engineView is NULL",
                static_cast<unsigned long long>(
                    m_diagnosticFrameNumber));
        }

        if (
            !m_pendingScriptPath.empty())
        {
            const std::string selectedScript =
                m_pendingScriptPath;

            m_pendingScriptPath.clear();

            diagnosticLog(
                "Deferred script load BEGIN: %s",
                selectedScript.c_str());

            if (
                loadScript(
                    selectedScript))
            {
                m_currentScriptPath =
                    selectedScript;

                diagnosticLog(
                    "Deferred script load SUCCESS.");
            }
            else {
                diagnosticLog(
                    "Deferred script load FAILED.");

                std::fprintf(
                    stderr,
                    "Engine script failed to load:\n%s\n",
                    selectedScript.c_str());
            }
        }

        /*
         * This is the most important diagnostic boundary.
         *
         * If the log reaches RENDER BEGIN but never RENDER END, we know
         * rendering itself got stuck.
         *
         * If it repeatedly reaches RENDER END while the physical screen
         * remains black, rendering is returning normally and we move
         * downstream toward the GPU present/drawable path.
         */
        if (verboseFrame) {
            diagnosticLog(
                "FRAME %llu: RENDER BEGIN",
                static_cast<unsigned long long>(
                    m_diagnosticFrameNumber));
        }

        renderScene();

        if (verboseFrame) {
            diagnosticLog(
                "FRAME %llu: RENDER END",
                static_cast<unsigned long long>(
                    m_diagnosticFrameNumber));
        }

        m_lastRenderTick =
            now;

        if (verboseFrame) {
            diagnosticLog(
                "FRAME %llu: tickProMotion EXIT",
                static_cast<unsigned long long>(
                    m_diagnosticFrameNumber));
        }

        return true;
    }

private:

    std::uint64_t m_diagnosticFrameNumber =
        0;
};

/*
 * ============================================================
 * Application state
 * ============================================================
 */

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

    std::uint64_t iterateCount =
        0;
};

static void printPathStatus(
    const RuntimePaths &paths)
{
    const bool mainFound =
        std::filesystem::exists(
            paths.assetDirectory
            / "main.mr");

    const bool fontFound =
        std::filesystem::exists(
            paths.assetDirectory
            / "fonts/slkscr.ttf");

    const bool meshFound =
        std::filesystem::exists(
            paths.assetDirectory
            / "authored_meshes.obj");

    const bool vertexFound =
        std::filesystem::exists(
            paths.assetDirectory
            / "shaders/engine_sim.vertex.msl");

    const bool fragmentFound =
        std::filesystem::exists(
            paths.assetDirectory
            / "shaders/engine_sim.fragment.msl");

    diagnosticLog(
        "Application directory: %s",
        paths.applicationDirectory
            .string()
            .c_str());

    diagnosticLog(
        "Asset directory: %s",
        paths.assetDirectory
            .string()
            .c_str());

    diagnosticLog(
        "Asset main.mr: %s",
        mainFound
            ? "FOUND"
            : "MISSING");

    diagnosticLog(
        "Asset font: %s",
        fontFound
            ? "FOUND"
            : "MISSING");

    diagnosticLog(
        "Asset mesh library: %s",
        meshFound
            ? "FOUND"
            : "MISSING");

    diagnosticLog(
        "Metal vertex shader: %s",
        vertexFound
            ? "FOUND"
            : "MISSING");

    diagnosticLog(
        "Metal fragment shader: %s",
        fragmentFound
            ? "FOUND"
            : "MISSING");

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
        mainFound
            ? "FOUND"
            : "MISSING");

    std::printf(
        "font: %s\n",
        fontFound
            ? "FOUND"
            : "MISSING");

    std::printf(
        "mesh library: %s\n",
        meshFound
            ? "FOUND"
            : "MISSING");

    std::printf(
        "Metal vertex shader: %s\n",
        vertexFound
            ? "FOUND"
            : "MISSING");

    std::printf(
        "Metal fragment shader: %s\n",
        fragmentFound
            ? "FOUND"
            : "MISSING");

    std::printf(
        "========================================\n");
}

/*
 * ============================================================
 * SDL application callbacks
 * ============================================================
 */

SDL_AppResult SDL_AppInit(
    void **appstate,
    int argc,
    char *argv[])
{
    (void)argc;
    (void)argv;

    /*
     * Open this before doing basically anything else.
     */
    openDiagnosticLog();

    diagnosticLog(
        "SDL_AppInit ENTER");

    diagnosticLog(
        "SDL version compiled: %d.%d.%d",
        SDL_MAJOR_VERSION,
        SDL_MINOR_VERSION,
        SDL_MICRO_VERSION);

    std::printf(
        "\n\n"
        "========================================\n"
        " ENGINE SIMULATOR iOS\n"
        " Diagnostic ProMotion host starting\n"
        "========================================\n");

    auto *state =
        new EngineSimIOSState();

    diagnosticLog(
        "EngineSimIOSState allocated: %p",
        static_cast<void *>(
            state));

    *appstate =
        state;

    diagnosticLog(
        "platform.initialize BEGIN");

    if (
        !state->platform.initialize(
            "Engine Simulator",
            1920,
            1080))
    {
        diagnosticLog(
            "platform.initialize FAILED: %s",
            state->platform
                .lastError()
                .c_str());

        std::fprintf(
            stderr,
            "iOS SDL platform initialization failed:\n%s\n",
            state->platform
                .lastError()
                .c_str());

        return SDL_APP_FAILURE;
    }

    diagnosticLog(
        "platform.initialize SUCCESS");

    diagnosticLog(
        "Window dimensions immediately after init: %dx%d",
        state->platform
            .windowWidth(),
        state->platform
            .windowHeight());

    diagnosticLog(
        "Native window handle: %p",
        state->platform
            .nativeWindowHandle());

    {
        std::error_code error;

        fs::create_directories(
            customEngineDirectory(),
            error);

        diagnosticLog(
            "Custom Engines directory creation: %s",
            error
                ? error.message().c_str()
                : "OK");
    }

    diagnosticLog(
        "RuntimePaths::discover BEGIN");

    const RuntimePaths paths =
        RuntimePaths::discover(
            state->platform
                .applicationDirectory(),
            {});

    diagnosticLog(
        "RuntimePaths::discover END");

    printPathStatus(
        paths);

    const fs::path shaderDirectory =
        paths.assetDirectory
        / "shaders";

    diagnosticLog(
        "Renderer shader directory: %s",
        shaderDirectory
            .string()
            .c_str());

    diagnosticLog(
        "renderer.initialize BEGIN");

    if (
        !state->renderer.initialize(
            state->platform
                .nativeWindowHandle(),
            shaderDirectory.string()))
    {
        diagnosticLog(
            "renderer.initialize FAILED: %s",
            state->renderer
                .lastError());

        std::fprintf(
            stderr,
            "EngineSim GPU renderer initialization failed:\n%s\n",
            state->renderer
                .lastError());

        return SDL_APP_FAILURE;
    }

    state->rendererInitialized =
        true;

    diagnosticLog(
        "renderer.initialize SUCCESS");

    std::printf(
        "EngineSim SDL GPU renderer initialized.\n");

    diagnosticLog(
        "application.initialize BEGIN");

    state->application.initialize(
        &state->platform,
        &state->renderer,
        &state->audioOutput,
        paths);

    diagnosticLog(
        "application.initialize END");

    state->applicationInitialized =
        true;

    diagnosticLog(
        "refreshEngineCatalog BEGIN");

    refreshEngineCatalog();

    diagnosticLog(
        "refreshEngineCatalog END");

    diagnosticLog(
        "Final startup window dimensions: %dx%d",
        state->platform
            .windowWidth(),
        state->platform
            .windowHeight());

    diagnosticLog(
        "SDL_AppInit returning SDL_APP_CONTINUE");

    std::printf(
        "========================================\n"
        " Real EngineSim application initialized\n"
        " Diagnostic logging enabled\n"
        " ProMotion presentation enabled\n"
        " Custom engine importing enabled\n"
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
        diagnosticLog(
            "SDL_AppEvent received null state/event.");

        return SDL_APP_CONTINUE;
    }

    /*
     * Log important lifecycle events.
     */
    switch (event->type) {

        case SDL_EVENT_TERMINATING:
        {
            diagnosticLog(
                "EVENT: SDL_EVENT_TERMINATING");

            state->terminating.store(
                true,
                std::memory_order_release);

            state->suspended.store(
                true,
                std::memory_order_release);

            diagnosticLog(
                "Returning SDL_APP_SUCCESS immediately for termination.");

            return SDL_APP_SUCCESS;
        }

        case SDL_EVENT_WILL_ENTER_BACKGROUND:
        {
            diagnosticLog(
                "EVENT: SDL_EVENT_WILL_ENTER_BACKGROUND");

            state->suspended.store(
                true,
                std::memory_order_release);

            break;
        }

        case SDL_EVENT_DID_ENTER_BACKGROUND:
        {
            diagnosticLog(
                "EVENT: SDL_EVENT_DID_ENTER_BACKGROUND");

            state->suspended.store(
                true,
                std::memory_order_release);

            break;
        }

        case SDL_EVENT_WILL_ENTER_FOREGROUND:
        {
            diagnosticLog(
                "EVENT: SDL_EVENT_WILL_ENTER_FOREGROUND");

            break;
        }

        case SDL_EVENT_DID_ENTER_FOREGROUND:
        {
            diagnosticLog(
                "EVENT: SDL_EVENT_DID_ENTER_FOREGROUND");

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
            diagnosticLog(
                "EVENT: SDL_EVENT_LOW_MEMORY");

            std::fprintf(
                stderr,
                "iOS lifecycle: low-memory warning.\n");

            break;
        }

        case SDL_EVENT_QUIT:
        {
            diagnosticLog(
                "EVENT: SDL_EVENT_QUIT");

            break;
        }

        case SDL_EVENT_DROP_FILE:
        {
            diagnosticLog(
                "EVENT: SDL_EVENT_DROP_FILE");

            break;
        }

        default:
            break;
    }

    if (
        event->type
            == SDL_EVENT_DROP_FILE
        && event->drop.data
            != nullptr)
    {
        const fs::path source(
            event->drop.data);

        diagnosticLog(
            "Received DROP_FILE path: %s",
            source.string().c_str());

        std::printf(
            "Received document from iOS:\n%s\n",
            source.string().c_str());

        const fs::path imported =
            importEngineFile(
                source);

        if (!imported.empty()) {
            diagnosticLog(
                "Refreshing engine catalog after import.");

            refreshEngineCatalog();

            if (
                state
                    ->applicationInitialized)
            {
                diagnosticLog(
                    "Requesting imported engine script.");

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

        diagnosticLog(
            "SDL_EVENT_QUIT -> SDL_APP_SUCCESS");

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
        diagnosticLog(
            "SDL_AppIterate failure: state=%p initialized=%d",
            static_cast<void *>(
                state),
            state != nullptr
                ? static_cast<int>(
                    state
                        ->applicationInitialized)
                : -1);

        return SDL_APP_FAILURE;
    }

    ++state->iterateCount;

    const bool verboseIteration =
        state->iterateCount <= 20
        || (
            state->iterateCount
            % 120
            == 0);

    if (verboseIteration) {
        diagnosticLog(
            "ITERATE %llu ENTER",
            static_cast<unsigned long long>(
                state->iterateCount));
    }

    if (
        state->terminating.load(
            std::memory_order_acquire))
    {
        diagnosticLog(
            "ITERATE %llu: terminating -> SUCCESS",
            static_cast<unsigned long long>(
                state->iterateCount));

        return SDL_APP_SUCCESS;
    }

    if (
        state->suspended.load(
            std::memory_order_acquire))
    {
        if (verboseIteration) {
            diagnosticLog(
                "ITERATE %llu: suspended",
                static_cast<unsigned long long>(
                    state->iterateCount));
        }

        return SDL_APP_CONTINUE;
    }

    if (
        state->resumePending.exchange(
            false,
            std::memory_order_acq_rel))
    {
        diagnosticLog(
            "ITERATE %llu: resume pending, resetting frame clock.",
            static_cast<unsigned long long>(
                state->iterateCount));

        state
            ->application
            .resetFrameClock();
    }

    if (verboseIteration) {
        diagnosticLog(
            "ITERATE %llu: tickProMotion BEGIN",
            static_cast<unsigned long long>(
                state->iterateCount));
    }

    const bool continueRunning =
        state
            ->application
            .tickProMotion();

    if (verboseIteration) {
        diagnosticLog(
            "ITERATE %llu: tickProMotion END result=%d",
            static_cast<unsigned long long>(
                state->iterateCount),
            static_cast<int>(
                continueRunning));
    }

    if (!continueRunning) {
        diagnosticLog(
            "tickProMotion requested termination.");

        state->terminating.store(
            true,
            std::memory_order_release);

        return SDL_APP_SUCCESS;
    }

    if (verboseIteration) {
        diagnosticLog(
            "ITERATE %llu EXIT",
            static_cast<unsigned long long>(
                state->iterateCount));
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(
    void *appstate,
    SDL_AppResult result)
{
    diagnosticLog(
        "SDL_AppQuit ENTER result=%d",
        static_cast<int>(
            result));

    auto *state =
        static_cast<
            EngineSimIOSState *>(
                appstate);

    if (state == nullptr) {
        diagnosticLog(
            "SDL_AppQuit: state NULL.");

        closeDiagnosticLog();

        return;
    }

    state->terminating.store(
        true,
        std::memory_order_release);

    state->suspended.store(
        true,
        std::memory_order_release);

    diagnosticLog(
        "Shutdown flags set.");

    std::printf(
        "EngineSim iOS shutting down.\n");

    if (
        state
            ->applicationInitialized)
    {
        diagnosticLog(
            "application.destroy BEGIN");

        state
            ->application
            .destroy();

        diagnosticLog(
            "application.destroy END");

        state
            ->applicationInitialized =
                false;
    }

    if (
        state
            ->rendererInitialized)
    {
        diagnosticLog(
            "renderer.shutdown BEGIN");

        state
            ->renderer
            .shutdown();

        diagnosticLog(
            "renderer.shutdown END");

        state
            ->rendererInitialized =
                false;
    }

    diagnosticLog(
        "platform.shutdown BEGIN");

    state
        ->platform
        .shutdown();

    diagnosticLog(
        "platform.shutdown END");

    diagnosticLog(
        "Deleting EngineSimIOSState.");

    delete state;

    diagnosticLog(
        "SDL_AppQuit complete.");

    closeDiagnosticLog();
}
