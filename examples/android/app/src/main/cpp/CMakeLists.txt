cmake_minimum_required(VERSION 3.22.1)
project(fpe_camerademo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FrameProcessorEngine: this example lives inside its repository.
set(FPE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..)
set(FPE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FPE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${FPE_DIR} fpe)

add_library(camerademo SHARED demo.cpp)
target_link_libraries(camerademo PRIVATE fpe::android camera2ndk log)
