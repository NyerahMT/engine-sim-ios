#ifndef ATG_ENGINE_SIM_RUNTIME_PATHS_H
#define ATG_ENGINE_SIM_RUNTIME_PATHS_H

#include <filesystem>

struct RuntimePaths {
    std::filesystem::path applicationDirectory;
    std::filesystem::path assetDirectory;

    // Packaged builds use the adjacent asset layout, while CMake builds can
    // point directly at the checked-out assets without a copied tree.
    static RuntimePaths discover(
        const std::filesystem::path &applicationDirectory,
        const std::filesystem::path &developmentAssetDirectory = {});
};

#endif /* ATG_ENGINE_SIM_RUNTIME_PATHS_H */
