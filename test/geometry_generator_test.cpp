#include <gtest/gtest.h>

#include "../include/geometry_generator.h"

TEST(GeometryGeneratorTests, GeneratesCircleUsingProjectOwnedVertexData) {
    GeometryGenerator generator;
    generator.initialize(64, 192);
    generator.startShape();

    GeometryGenerator::Circle2dParameters parameters;
    parameters.center_x = 4.0f;
    parameters.center_y = -2.0f;
    parameters.radius = 3.0f;
    parameters.maxEdgeLength = 1.0f;

    ASSERT_TRUE(generator.generateCircle2d(parameters));
    EXPECT_GT(generator.getCurrentVertexCount(), 3);
    EXPECT_EQ(generator.getCurrentIndexCount() % 3, 0);
    EXPECT_FLOAT_EQ(generator.getVertexData()[0].Pos.x, 4.0f);
    EXPECT_FLOAT_EQ(generator.getVertexData()[0].Pos.y, -2.0f);
    EXPECT_FLOAT_EQ(generator.getVertexData()[0].Normal.z, 1.0f);

    generator.destroy();
}
