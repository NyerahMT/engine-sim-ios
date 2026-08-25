#include "../include/engine_catalog.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;

std::vector<EngineCatalogEntry> g_catalog;
bool g_catalogInitialized = false;

std::string titleize(std::string value) {
    std::replace(value.begin(), value.end(), '_', ' ');
    std::replace(value.begin(), value.end(), '-', ' ');

    bool capitalize = true;

    for (char &c : value) {
        if (c == ' ') {
            capitalize = true;
        }
        else if (capitalize) {
            c = static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(c)));

            capitalize = false;
        }
    }

    return value;
}

std::string engineNameFromPath(const fs::path &path) {
    std::string filename =
        path.stem().string();

    const std::size_t prefixEnd =
        filename.find('_');

    if (
        prefixEnd != std::string::npos
        && std::all_of(
            filename.begin(),
            filename.begin() + prefixEnd,
            [](unsigned char c) {
                return std::isdigit(c);
            }))
    {
        filename.erase(
            0,
            prefixEnd + 1);
    }

    return titleize(filename);
}

EngineCatalogEntry makeBundledEntry(
    const char *relativePath)
{
    const std::string path(relativePath);

    const std::size_t groupStart =
        std::string("engines/").size();

    const std::size_t groupEnd =
        path.find('/', groupStart);

    const std::size_t filenameStart =
        path.rfind('/') + 1;

    std::string filename =
        path.substr(filenameStart);

    if (
        filename.size() >= 3
        && filename.substr(
            filename.size() - 3)
            == ".mr")
    {
        filename.erase(
            filename.size() - 3);
    }

    const std::size_t prefixEnd =
        filename.find('_');

    if (
        prefixEnd != std::string::npos
        && std::all_of(
            filename.begin(),
            filename.begin() + prefixEnd,
            [](unsigned char c) {
                return std::isdigit(c);
            }))
    {
        filename.erase(
            0,
            prefixEnd + 1);
    }

    return {
        titleize(
            path.substr(
                groupStart,
                groupEnd - groupStart)),
        titleize(filename),
        path
    };
}

fs::path customEngineDirectory() {
#if defined(ENGINE_SIM_IOS)

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

#else

    return {};

#endif
}

bool isMrFile(const fs::path &path) {
    std::string extension =
        path.extension().string();

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

bool obviousLibraryFile(const fs::path &path) {
    std::string stem =
        path.stem().string();

    std::transform(
        stem.begin(),
        stem.end(),
        stem.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    return
        stem == "engine_sim"
        || stem == "utilities"
        || stem == "constants"
        || stem == "units";
}

void appendCustomEngines(
    std::vector<EngineCatalogEntry> &catalog)
{
#if defined(ENGINE_SIM_IOS)

    const fs::path root =
        customEngineDirectory();

    if (root.empty()) {
        return;
    }

    std::error_code error;

    /*
     * Always create this directory.
     *
     * With UIFileSharingEnabled the user will see:
     *
     * Files
     *   On My iPhone
     *     Engine Simulator
     *       Custom Engines
     */
    fs::create_directories(
        root,
        error);

    error.clear();

    if (
        !fs::exists(root, error)
        || error)
    {
        return;
    }

    std::vector<EngineCatalogEntry> custom;

    fs::recursive_directory_iterator it(
        root,
        fs::directory_options::skip_permission_denied,
        error);

    const fs::recursive_directory_iterator end;

    while (
        !error
        && it != end)
    {
        const fs::directory_entry &file =
            *it;

        std::error_code fileError;

        if (
            file.is_regular_file(fileError)
            && !fileError
            && isMrFile(file.path())
            && !obviousLibraryFile(file.path()))
        {
            custom.push_back({
                "Downloaded Engines",
                engineNameFromPath(
                    file.path()),
                /*
                 * Absolute paths are intentional.
                 *
                 * std::filesystem path composition keeps an
                 * absolute RHS absolute when loadScript builds
                 * the final source path.
                 */
                file.path().string()
            });
        }

        it.increment(error);
    }

    std::sort(
        custom.begin(),
        custom.end(),
        [](
            const EngineCatalogEntry &a,
            const EngineCatalogEntry &b)
        {
            if (a.name != b.name) {
                return a.name < b.name;
            }

            return
                a.relativeScriptPath
                < b.relativeScriptPath;
        });

    catalog.insert(
        catalog.end(),
        custom.begin(),
        custom.end());

#endif
}

std::vector<EngineCatalogEntry> buildCatalog() {
    const char *paths[] = {
#include "engine_catalog_paths.inc"
    };

    std::vector<EngineCatalogEntry> result;

    result.reserve(
        sizeof(paths) / sizeof(paths[0])
        + 64);

    for (const char *path : paths) {
        result.push_back(
            makeBundledEntry(path));
    }

    appendCustomEngines(result);

    return result;
}

}

void refreshEngineCatalog() {
    g_catalog =
        buildCatalog();

    g_catalogInitialized =
        true;
}

const std::vector<EngineCatalogEntry> &
engineCatalog()
{
    if (!g_catalogInitialized) {
        refreshEngineCatalog();
    }

    return g_catalog;
}
