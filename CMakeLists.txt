cmake_minimum_required(VERSION 3.14)
project(FrameProcessorEngineLib VERSION 0.1 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)

# Examples
add_executable(example examples/example.cpp)
target_link_libraries(example PRIVATE FrameProcessorEngineLib)

# Enable testing
enable_testing()
