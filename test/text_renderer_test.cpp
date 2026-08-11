#include "text_renderer.h"

#include <gtest/gtest.h>

TEST(TextRenderer, MeasuresLinesAndEmitsBitmapRuns) {
    TextRenderer renderer;
    ASSERT_TRUE(renderer.loadFont(
        std::string(ENGINE_SIM_TEST_SOURCE_DIRECTORY)
        + "/assets/fonts/slkscr.ttf"));
    int runCount = 0;
    float coveredWidth = 0.0f;
    float firstRunX = 0.0f;
    renderer.setRenderCallback([&](const std::vector<TextRenderer::Run> &runs, const ysVector &) {
        runCount += static_cast<int>(runs.size());
        if (firstRunX == 0.0f && !runs.empty()) firstRunX = runs.front().x;
        for (const TextRenderer::Run &run : runs) coveredWidth += run.width;
    });

    renderer.SetColor(ysMath::Constants::One);
    renderer.RenderText("E1", 10.0f, 20.0f, 14.0f);

    EXPECT_GT(runCount, 0);
    EXPECT_GT(coveredWidth, 0.0f);
    EXPECT_GT(firstRunX, 10.0f);
    EXPECT_FLOAT_EQ(renderer.CalculateWidth("A\n12", 14.0f), 18.333334f);
}
