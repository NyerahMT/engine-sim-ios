#include "../include/runtime_paths.h"

RuntimePaths RuntimePaths::discover(
    const std::filesystem::path &applicationDirectory,
    const std::filesystem::path &developmentAssetDirectory)
{
    RuntimePaths paths;
    paths.applicationDirectory = applicationDirectory;
    paths.assetDirectory = applicationDirectory / "../assets";

    if (!developmentAssetDirectory.empty()
        && !std::filesystem::exists(paths.assetDirectory)
        && std::filesystem::exists(developmentAssetDirectory)) {
        paths.assetDirectory = developmentAssetDirectory;
    }

    return paths;
}
