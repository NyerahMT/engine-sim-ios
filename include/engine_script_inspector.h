#ifndef ENGINE_SCRIPT_INSPECTOR_H
#define ENGINE_SCRIPT_INSPECTOR_H

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace engine_script {

inline std::string sanitizedSource(const std::filesystem::path &path)
{
    std::ifstream file(
        path,
        std::ios::in
            | std::ios::binary);

    if (!file.is_open()) {
        return {};
    }

    std::string source(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    std::string cleaned;
    cleaned.reserve(source.size());

    bool lineComment = false;
    bool blockComment = false;
    bool quotedString = false;
    bool escapeNext = false;

    for (
        std::size_t i = 0;
        i < source.size();
        ++i)
    {
        const char c = source[i];
        const char next =
            (i + 1 < source.size())
                ? source[i + 1]
                : '\0';

        if (lineComment) {
            if (c == '\n') {
                lineComment = false;
                cleaned.push_back('\n');
            }

            continue;
        }

        if (blockComment) {
            if (
                c == '*'
                && next == '/')
            {
                blockComment = false;
                ++i;
            }

            continue;
        }

        if (quotedString) {
            if (escapeNext) {
                escapeNext = false;
                continue;
            }

            if (c == '\\') {
                escapeNext = true;
                continue;
            }

            if (c == '"') {
                quotedString = false;
            }

            continue;
        }

        if (
            c == '/'
            && next == '/')
        {
            lineComment = true;
            ++i;
            continue;
        }

        if (
            c == '/'
            && next == '*')
        {
            blockComment = true;
            ++i;
            continue;
        }

        if (c == '"') {
            quotedString = true;
            continue;
        }

        cleaned.push_back(c);
    }

    return cleaned;
}

inline std::vector<std::string> tokens(
    const std::filesystem::path &path)
{
    const std::string source =
        sanitizedSource(path);

    std::vector<std::string> result;
    result.reserve(512);

    std::string token;

    auto finishToken =
        [&]()
        {
            if (!token.empty()) {
                result.push_back(token);
                token.clear();
            }
        };

    for (const char c : source) {
        if (
            std::isalnum(
                static_cast<unsigned char>(c))
            || c == '_')
        {
            token.push_back(c);
            continue;
        }

        finishToken();

        if (
            c == '{'
            || c == '}'
            || c == ':'
            || c == ';')
        {
            result.emplace_back(
                1,
                c);
        }
    }

    finishToken();

    return result;
}

/*
 * Loader B: identify the legacy engine-definition module format.
 *
 * The first compatibility target is M52B28.mr:
 *
 *     public node M52B28 {
 *         alias output __out: engine;
 *         ...
 *     }
 *
 * Classic scripts with public node main remain Loader A territory.
 * If more than one engine-output public node exists, decline to guess.
 */
inline std::string findSingleEngineModuleNode(
    const std::filesystem::path &path)
{
    const std::vector<std::string> sourceTokens =
        tokens(path);

    for (
        std::size_t i = 0;
        i + 2 < sourceTokens.size();
        ++i)
    {
        if (
            sourceTokens[i] == "public"
            && sourceTokens[i + 1] == "node"
            && sourceTokens[i + 2] == "main")
        {
            return {};
        }
    }

    std::string candidate;

    for (
        std::size_t i = 0;
        i + 3 < sourceTokens.size();
        ++i)
    {
        if (
            sourceTokens[i] != "public"
            || sourceTokens[i + 1] != "node")
        {
            continue;
        }

        const std::string nodeName =
            sourceTokens[i + 2];

        std::size_t openBrace =
            i + 3;

        while (
            openBrace < sourceTokens.size()
            && sourceTokens[openBrace] != "{")
        {
            ++openBrace;
        }

        if (openBrace == sourceTokens.size()) {
            continue;
        }

        int depth = 1;
        bool outputsEngine = false;

        for (
            std::size_t j = openBrace + 1;
            j < sourceTokens.size()
                && depth > 0;
            ++j)
        {
            if (sourceTokens[j] == "{") {
                ++depth;
                continue;
            }

            if (sourceTokens[j] == "}") {
                --depth;
                continue;
            }

            if (
                depth == 1
                && j + 4 < sourceTokens.size()
                && sourceTokens[j] == "alias"
                && sourceTokens[j + 1] == "output"
                && sourceTokens[j + 2] == "__out"
                && sourceTokens[j + 3] == ":"
                && sourceTokens[j + 4] == "engine")
            {
                outputsEngine = true;
            }
        }

        if (!outputsEngine) {
            continue;
        }

        if (candidate.empty()) {
            candidate = nodeName;
        }
        else if (candidate != nodeName) {
            return {};
        }
    }

    return candidate;
}

} // namespace engine_script

#endif
