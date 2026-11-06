#include <fpe/version.hpp>

#include <gtest/gtest.h>

TEST(Version, MatchesMacros) {
  EXPECT_EQ(fpe::version.major, FPE_VERSION_MAJOR);
  EXPECT_EQ(fpe::version.minor, FPE_VERSION_MINOR);
  EXPECT_EQ(fpe::version.patch, FPE_VERSION_PATCH);
}
