include(FetchContent)

# GoogleTest is a test-only dependency. Offline builds can point
# FETCHCONTENT_SOURCE_DIR_GOOGLETEST at a local checkout.
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
  GIT_SHALLOW TRUE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(fpe_tests
  support/alloc_guard.cpp
  test_alloc_guard.cpp
  test_frame_store.cpp
  test_latest_mailbox.cpp
  test_mpmc_queue.cpp
  test_engine.cpp
  test_version.cpp
)
# The Android adapter is compiled for the host against a fake NDK (same C signatures).
if(NOT ANDROID)
  target_sources(fpe_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/platform/android/src/camera.cpp
    android_host/fake_ndk.cpp
    test_android_camera.cpp)
  target_include_directories(fpe_tests BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/android_host/fake_ndk
    ${PROJECT_SOURCE_DIR}/platform/android/include)
endif()
target_include_directories(fpe_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fpe_tests PRIVATE fpe::fpe GTest::gtest_main)
fpe_configure_target(fpe_tests)

gtest_discover_tests(fpe_tests DISCOVERY_TIMEOUT 60)
