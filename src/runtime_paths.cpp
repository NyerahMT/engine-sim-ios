#include "../include/runtime_paths.h"

#include <array>

RuntimePaths RuntimePaths::discover(
    const std::filesystem::path &applicationDirectory,
    const std::filesystem::path &developmentAssetDirectory)
{
    RuntimePaths paths;
    paths.applicationDirectory = applicationDirectory;
    // SDL may report the executable's directory, Contents/Resources, or the
    // bundle root on macOS. Resolve the packaged assets from each supported
    // base-path shape before falling back to the source tree.
    const std::array<std::filesystem::path, 3> packagedAssetDirectories = {
        applicationDirectory / "../assets",
        applicationDirectory / "assets",
        applicationDirectory / "Contents/assets"
    };
    paths.assetDirectory = packagedAssetDirectories.front();
    for (const auto &candidate : packagedAssetDirectories) {
        if (std::filesystem::exists(candidate)) {
            paths.assetDirectory = candidate;
            break;
        }
    }

    if (!developmentAssetDirectory.empty()
        && !std::filesystem::exists(paths.assetDirectory)
        && std::filesystem::exists(developmentAssetDirectory)) {
        paths.assetDirectory = developmentAssetDirectory;
    }

    return paths;
}
