#include "../include/desktop_platform_sdl.h"
#include "../include/engine_sim_application.h"
#include "../include/runtime_paths.h"
#include "../include/sdl_gpu_renderer.h"
#include "../include/sdl_audio_output.h"

#include <cstdio>
#include <filesystem>

int main(int, char **) {
    DesktopPlatformSdl platform;
    if (!platform.initialize("Open Engine Simulator", 1920, 1080)) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", platform.lastError().c_str());
        return 1;
    }

    const RuntimePaths paths = RuntimePaths::discover(
        platform.applicationDirectory(), ENGINE_SIM_SOURCE_ASSET_DIRECTORY);
    const std::filesystem::path packagedShaderDirectory = paths.assetDirectory / "shaders";
    const std::string shaderDirectory = std::filesystem::exists(
            packagedShaderDirectory / "engine_sim.vertex.msl")
        ? packagedShaderDirectory.string()
        : ENGINE_SIM_SHADER_DIRECTORY;

    SdlGpuRenderer renderer;
    if (!renderer.initialize(platform.nativeWindowHandle(), shaderDirectory)) {
        std::fprintf(stderr, "SDL GPU initialization failed: %s\n", renderer.lastError());
        return 1;
    }

    EngineSimApplication application;
    SdlAudioOutput audioOutput;
    application.initialize(&platform, &renderer, &audioOutput, paths);
    application.run();
    application.destroy();

    renderer.shutdown();
    platform.shutdown();
    return 0;
}
