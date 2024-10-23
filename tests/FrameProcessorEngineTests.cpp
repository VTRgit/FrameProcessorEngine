#include <gtest/gtest.h>
#include <FrameProcessorEngine.h>

TEST(MyCppLibTests, AddFunction) {
    EXPECT_EQ(add(1, 1), 2);
    EXPECT_EQ(add(-1, 1), 0);
    EXPECT_EQ(add(-1, -1), -2);
}
