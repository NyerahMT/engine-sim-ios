#include "../include/shaders.h"

#include <gtest/gtest.h>

TEST(ShadersTests, PreservesSceneAndUiStagesWithoutBackendTypes) {
    Shaders shaders;

    EXPECT_NE(shaders.GetRegularFlags(), shaders.GetUiFlags());
    EXPECT_EQ(shaders.GetRegularFlags(), Shaders::SceneStage);
    EXPECT_EQ(shaders.GetUiFlags(), Shaders::UiStage);
}

TEST(ShadersTests, StoresObjectAndClearState) {
    Shaders shaders;
    const ysVector color = ysColor::srgbiToLinear(0x123456);
    const ysMatrix transform = ysMath::TranslationTransform(ysMath::LoadVector(3.0f, 4.0f, 5.0f));

    shaders.SetBaseColor(color);
    shaders.SetObjectTransform(transform);
    shaders.SetClearColor(color);

    EXPECT_FLOAT_EQ(shaders.m_objectVariables.BaseColor.x, color.x);
    EXPECT_FLOAT_EQ(shaders.m_objectVariables.Transform.m[0][3], 3.0f);
    EXPECT_FLOAT_EQ(shaders.m_objectVariables.Transform.m[1][3], 4.0f);
    EXPECT_FLOAT_EQ(shaders.GetClearColor().z, color.z);
}
