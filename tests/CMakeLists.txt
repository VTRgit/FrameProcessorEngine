# Add Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Add the test executable
add_executable(FrameProcessorEngineLibTests
  FrameProcessorEngineTests.cpp
  RingBufferMultiThreadTests.cpp
  RingBufferTests.cpp
)

# Link Google Test and the library
target_link_libraries(FrameProcessorEngineLibTests gtest_main FrameProcessorEngineLib)

# Add tests
include(GoogleTest)
gtest_discover_tests(FrameProcessorEngineLibTests)
