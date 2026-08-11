#include "compiler.h"

#include <gtest/gtest.h>

#include <filesystem>

TEST(ScriptCompileTest, CompilesTheBundledEntryScript) {
    es_script::Compiler compiler;
    compiler.initialize(ENGINE_SIM_TEST_ASSET_DIRECTORY);

    const std::filesystem::path script =
        std::filesystem::path(ENGINE_SIM_TEST_ASSET_DIRECTORY) / "main.mr";
    EXPECT_TRUE(compiler.compile(script.string()));

    compiler.destroy();
}
