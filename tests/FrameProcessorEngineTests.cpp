#include <FrameProcessorEngineInterface.h>
#include <gtest/gtest.h>

using namespace FPE;

TEST(FrameProcessorEngineTests, StartStop) {
  ASSERT_EQ(startFPE(), Response::noError);
  ASSERT_EQ(stopFPE(), Response::noError);
}
