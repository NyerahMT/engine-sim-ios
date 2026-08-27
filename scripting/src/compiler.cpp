#include "../include/compiler.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <cstdlib>

namespace {

namespace fs = std::filesystem;

/*
 * Engine Simulator's picker creates a tiny temporary entry point:
 *
 *     import "/absolute/path/to/custom_engine.mr"
 *
 *     main()
 *
 * For downloaded engines we want Piranha to also search beside the
 * imported engine file. This allows things like:
 *
 *     import "heads.mr"
 *     import "cams.mr"
 *
 * to work inside downloaded engine packages.
 */
fs::path findImportedEntryPoint(
    const fs::path &entryPoint)
{
    std::ifstream file(entryPoint);

    if (!file.is_open()) {
        return {};
    }

    std::string line;

    while (std::getline(file, line)) {
        const std::size_t importPosition =
            line.find("import");

        if (importPosition == std::string::npos) {
            continue;
        }

        const std::size_t quoteStart =
            line.find(
                '"',
                importPosition);

        if (quoteStart == std::string::npos) {
            continue;
        }

        const std::size_t quoteEnd =
            line.find(
                '"',
                quoteStart + 1);

        if (quoteEnd == std::string::npos) {
            continue;
        }

        const std::string imported =
            line.substr(
                quoteStart + 1,
                quoteEnd - quoteStart - 1);

        if (imported.empty()) {
            continue;
        }

        return fs::path(imported);
    }

    return {};
}

}

es_script::Compiler::Output *
es_script::Compiler::s_output =
    nullptr;

es_script::Compiler::Compiler() {
    m_compiler =
        nullptr;
}

es_script::Compiler::~Compiler() {
    assert(
        m_compiler
        == nullptr);
}

es_script::Compiler::Output *
es_script::Compiler::output()
{
    if (s_output == nullptr) {
        s_output =
            new Output;
    }

    return s_output;
}

void es_script::Compiler::initialize(
    const std::string &assetDirectory)
{
    m_compiler =
        new piranha::Compiler(
            &m_rules);

    m_compiler->setFileExtension(
        ".mr");

    /*
     * Original Engine Simulator library.
     *
     * This resolves:
     *
     *     import "engine_sim.mr"
     *     import "constants/units.mr"
     *     import "sound-library/..."
     */
    m_compiler->addSearchPath(
        assetDirectory
        + "/es/");

    /*
     * Also expose the asset root.
     *
     * Community engines sometimes reference files using paths such as:
     *
     *     engines/...
     *     es/...
     *
     * rather than relying exclusively on engine_sim.mr.
     */
    m_compiler->addSearchPath(
        assetDirectory
        + "/");

    /*
     * And expose bundled engines as an additional compatibility root.
     */
    m_compiler->addSearchPath(
        assetDirectory
        + "/engines/");

    m_rules.initialize();
}

bool es_script::Compiler::compile(
    const piranha::IrPath &path)
{
    /*
     * Add the entrypoint's directory.
     *
     * This is useful for ordinary direct script compilation.
     */
    const fs::path entryPath(
        path.toString());

    if (
        !entryPath.empty()
        && entryPath.has_parent_path())
    {
        m_compiler->addSearchPath(
            entryPath
                .parent_path()
                .string());
    }

    /*
     * EngineSim's engine picker normally compiles a GENERATED temporary
     * script rather than the selected engine directly.
     *
     * Find that generated script's absolute import and add the selected
     * engine's directory to Piranha's search paths.
     *
     * This is the important custom-engine fix.
     */
    const fs::path importedPath =
        findImportedEntryPoint(
            entryPath);

    if (
        !importedPath.empty()
        && importedPath
            .has_parent_path())
    {
        m_compiler->addSearchPath(
            importedPath
                .parent_path()
                .string());
    }

    bool successful =
        false;

    /*
     * Keep the error log beside the process working directory.
     *
     * On iOS stdout/stderr will also tell us which script failed,
     * while the normal EngineSim UI safely leaves the old engine loaded.
     */
    fs::path errorLogPath =
        "error_log.log";

    const char *home =
        std::getenv("HOME");

    if (
        home != nullptr
        && home[0] != '\0')
    {
        const fs::path documents =
            fs::path(home)
            / "Documents";

        std::error_code documentsError;

        if (
            fs::exists(
                documents,
                documentsError)
            && !documentsError)
        {
            errorLogPath =
                documents
                / "engine-sim.log";
        }
    }

    std::ofstream file(
        errorLogPath,
        std::ios::out
            | std::ios::app);

    if (file.is_open()) {
        file
            << "[Compiler] BEGIN "
            << path.toString()
            << std::endl;
    }

    piranha::IrCompilationUnit *unit =
        m_compiler->compile(path);

    if (unit == nullptr) {
        file
            << "[Compiler] Can't find file: "
            << path.toString()
            << "\n";
    }
    else {
        const piranha::ErrorList *errors =
            m_compiler
                ->getErrorList();

        if (
            errors->getErrorCount()
            == 0)
        {
            unit->build(
                &m_program);

            m_program.initialize();

            successful =
                true;
        }
        else {
            for (
                int i = 0;
                i < errors
                    ->getErrorCount();
                ++i)
            {
                printError(
                    errors
                        ->getCompilationError(
                            i),
                    file);
            }
        }
    }

    if (file.is_open()) {
        file
            << "[Compiler] "
            << (successful ? "SUCCESS" : "FAILED")
            << " "
            << path.toString()
            << std::endl;
    }

    file.close();

    return successful;
}

es_script::Compiler::Output
es_script::Compiler::execute()
{
    const bool result =
        m_program.execute();

    if (!result) {
        /*
         * Runtime errors are reported by Piranha.
         */
    }

    return *output();
}

void es_script::Compiler::destroy() {
    m_program.free();

    m_compiler->free();

    delete m_compiler;

    m_compiler =
        nullptr;
}

void es_script::Compiler::printError(
    const piranha::CompilationError *err,
    std::ofstream &file) const
{
    const piranha::ErrorCode_struct
        &errorCode =
            err->getErrorCode();

    file
        << err
            ->getCompilationUnit()
            ->getPath()
            .getStem()
        << "("
        << err
            ->getErrorLocation()
            ->lineStart
        << "): error "
        << errorCode.stage
        << errorCode.code
        << ": "
        << errorCode.info
        << std::endl;

    piranha::IrContextTree *context =
        err->getInstantiation();

    while (context != nullptr) {
        piranha::IrNode *instance =
            context->getContext();

        if (instance != nullptr) {
            const std::string instanceName =
                instance->getName();

            const std::string definitionName =
                (
                    instance->getDefinition()
                    != nullptr)
                ? instance
                    ->getDefinition()
                    ->getName()
                : "<Type Error>";

            const std::string formattedName =
                instanceName.empty()
                ? "<unnamed> "
                    + definitionName
                : instanceName
                    + " "
                    + definitionName;

            file
                << "       While instantiating: "
                << instance
                    ->getParentUnit()
                    ->getPath()
                    .getStem()
                << "("
                << instance
                    ->getSummaryToken()
                    ->lineStart
                << "): "
                << formattedName
                << std::endl;
        }

        context =
            context->getParent();
    }
}
