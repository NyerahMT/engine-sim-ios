#include "../include/authored_mesh_library.h"

#include <gtest/gtest.h>

#include <string>

namespace {
std::string sourceAsset(const char *name) {
    return std::string(ENGINE_SIM_TEST_SOURCE_DIRECTORY) + "/assets/" + name;
}
}

TEST(AuthoredMeshLibrary, LoadsBlenderExportedEngineMeshes) {
    AuthoredMeshLibrary meshes;
    ASSERT_TRUE(meshes.load(sourceAsset("authored_meshes.obj")));

    struct Expectation { const char *name; std::size_t vertices; std::size_t faces; };
    constexpr Expectation expected[] = {
        { "Crankshaft", 663, 1240 },
        { "ConnectingRod", 476, 776 },
        { "Piston", 1485, 2736 },
        { "CylinderHead", 4195, 7984 },
        { "Valve", 1625, 3072 },
        { "CrankSnoutThreads", 33, 32 },
        { "CrankSnout", 191, 252 },
    };

    for (const Expectation &entry : expected) {
        const AuthoredMeshLibrary::Mesh *mesh = meshes.find(entry.name);
        ASSERT_NE(mesh, nullptr) << entry.name;
        EXPECT_EQ(mesh->vertices.size(), entry.vertices) << entry.name;
        EXPECT_EQ(mesh->indices.size(), entry.faces * 3) << entry.name;
    }

    // The logo is intentionally authored and replaceable in Blender, so its
    // exact topology is not a stable engine-geometry regression value.
    const AuthoredMeshLibrary::Mesh *logo = meshes.find("Logo");
    ASSERT_NE(logo, nullptr);
    EXPECT_FALSE(logo->vertices.empty());
    EXPECT_FALSE(logo->indices.empty());
}
