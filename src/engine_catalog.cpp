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

std::string titleize(std::string value) {
    std::replace(
        value.begin(),
        value.end(),
        '_',
        ' ');

    std::replace(
        value.begin(),
        value.end(),
        '-',
        ' ');

    bool capitalize = true;

    for (char &character : value) {
        if (character == ' ') {
            capitalize = true;
        }
        else if (capitalize) {
            character =
                static_cast<char>(
                    std::toupper(
                        static_cast<unsigned char>(
                            character)));

            capitalize = false;
        }
    }

    return value;
}

std::string engineNameFromPath(
    const fs::path &path)
{
    std::string filename =
        path.stem().string();

    const size_t numericPrefixEnd =
        filename.find('_');

    if (
        numericPrefixEnd
            != std::string::npos
        && std::all_of(
            filename.begin(),
            filename.begin()
                + numericPrefixEnd,
            [](unsigned char c) {
                return std::isdigit(c);
            }))
    {
        filename.erase(
            0,
            numericPrefixEnd + 1);
    }

    return titleize(filename);
}

EngineCatalogEntry makeBundledEntry(
    const char *relativePath)
{
    const std::string path(
        relativePath);

    const size_t groupStart =
        std::string(
            "engines/")
            .size();

    const size_t groupEnd =
        path.find(
            '/',
            groupStart);

    const size_t filenameStart =
        path.rfind('/')
        + 1;

    std::string filename =
        path.substr(
            filenameStart);

    if (
        filename.size() >= 3)
    {
        filename.erase(
            filename.size() - 3);
    }

    const size_t numericPrefixEnd =
        filename.find('_');

    if (
        numericPrefixEnd
            != std::string::npos
        && std::all_of(
            filename.begin(),
            filename.begin()
                + numericPrefixEnd,
            [](unsigned char c) {
                return std::isdigit(c);
            }))
    {
        filename.erase(
            0,
            numericPrefixEnd + 1);
    }

    return {
        titleize(
            path.substr(
                groupStart,
                groupEnd
                    - groupStart)),
        titleize(filename),
        path
    };
}

fs::path customEngineDirectory() {
#if defined(ENGINE_SIM_IOS)

    /*
     * iOS gives the app a writable data container.
     *
     * HOME points at:
     *
     * /var/mobile/Containers/Data/Application/<UUID>
     *
     * Documents is exposed to the Files app once
     * UIFileSharingEnabled is enabled in Info.plist.
     */
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

    /*
     * Keep desktop behavior harmless.
     *
     * A desktop custom-engine directory can be added later
     * without changing the iOS implementation.
     */
    return {};

#endif
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

bool looksLikeHelperScript(
    const fs::path &path)
{
    /*
     * Don't clutter the picker with the core library or obvious
     * helper/include files from downloaded engine packages.
     *
     * A package's main engine script still appears normally.
     */
    const std::string stem =
        path.stem()
            .string();

    if (
        stem == "engine_sim"
        || stem == "utilities"
        || stem == "constants"
        || stem == "units")
    {
        return true;
    }

    return false;
}

void appendCustomEngines(
    std::vector<EngineCatalogEntry> &result)
{
#if defined(ENGINE_SIM_IOS)

    const fs::path root =
        customEngineDirectory();

    if (root.empty()) {
        return;
    }

    std::error_code error;

    /*
     * Create the folder automatically.
     *
     * Once the app has launched once, Files will show:
     *
     * On My iPhone
     *   Engine Simulator
     *     Custom Engines
     */
    fs::create_directories(
        root,
        error);

    error.clear();

    if (
        !fs::exists(
            root,
            error))
    {
        return;
    }

    std::vector<
        EngineCatalogEntry>
        custom;

    fs::recursive_directory_iterator iterator(
        root,
        fs::directory_options::
            skip_permission_denied,
        error);

    const fs::recursive_directory_iterator end;

    while (
        !error
        && iterator != end)
    {
        const fs::directory_entry &entry =
            *iterator;

        std::error_code entryError;

        if (
            entry.is_regular_file(
                entryError)
            && !entryError
            && isMrFile(
                entry.path())
            && !looksLikeHelperScript(
                entry.path()))
        {
            /*
             * The application loader already supports this.
             *
             * std::filesystem::path(assetPath) / absolutePath
             * resolves to absolutePath, so no special loading path
             * is necessary.
             */
            custom.push_back({
                "Downloaded Engines",
                engineNameFromPath(
                    entry.path()),
                entry.path()
                    .string()
            });
        }

        iterator.increment(
            error);
    }

    /*
     * Stable alphabetical list regardless of Files/iOS
     * directory enumeration order.
     */
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

    result.insert(
        result.end(),
        custom.begin(),
        custom.end());

#endif
}

std::vector<EngineCatalogEntry>
buildCatalog()
{
    const char *paths[] = {
#include "engine_catalog_paths.inc"
    };

    std::vector<
        EngineCatalogEntry>
        result;

    result.reserve(
        sizeof(paths)
            / sizeof(paths[0])
        + 32);

    for (
        const char *path
        : paths)
    {
        result.push_back(
            makeBundledEntry(
                path));
    }

    appendCustomEngines(
        result);

    return result;
}

}

const std::vector<EngineCatalogEntry> &
engineCatalog()
{
    /*
     * Deliberately rescan on every call.
     *
     * Engine catalogs are tiny, and this means a user can:
     *
     * 1. background Engine Simulator
     * 2. save an engine in Files
     * 3. return
     * 4. reopen the engine picker
     *
     * without restarting the app.
     *
     * The returned reference remains valid until the next call.
     */
    static std::vector<
        EngineCatalogEntry>
        catalog;

    catalog =
        buildCatalog();

    return catalog;
}
