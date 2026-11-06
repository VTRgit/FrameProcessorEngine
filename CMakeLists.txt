cmake_minimum_required(VERSION 3.21)
project(fpe VERSION 0.2.0 LANGUAGES CXX)

set(FPE_IS_TOP_LEVEL ${PROJECT_IS_TOP_LEVEL})

option(FPE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(FPE_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(FPE_TSAN "Build with ThreadSanitizer (incompatible with FPE_SANITIZE)" OFF)
option(FPE_BUILD_TESTS "Build the tests" ${FPE_IS_TOP_LEVEL})
option(FPE_BUILD_EXAMPLES "Build the examples" ${FPE_IS_TOP_LEVEL})

if(FPE_SANITIZE AND FPE_TSAN)
  message(FATAL_ERROR "FPE_SANITIZE and FPE_TSAN cannot be enabled together")
endif()

include(cmake/FpeOptions.cmake)

find_package(Threads REQUIRED)

add_library(fpe
  src/engine.cpp
  src/frame_store.cpp
)
add_library(fpe::fpe ALIAS fpe)
target_include_directories(fpe PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(fpe PUBLIC cxx_std_20)
target_link_libraries(fpe PUBLIC Threads::Threads)
fpe_configure_target(fpe)

if(ANDROID)
  add_subdirectory(platform/android)
endif()

if(FPE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(FPE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
