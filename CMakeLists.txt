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

# Function to enable sanitizers
function(enable_sanitizers target)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
        # Enable different sanitizers based on configuration
        set(SANITIZER_FLAGS "-fsanitize=address,undefined") # Adjust as needed
        target_compile_options(${target} PRIVATE ${SANITIZER_FLAGS})
        target_link_options(${target} PRIVATE ${SANITIZER_FLAGS})
    else()
        message(WARNING "Sanitizers are only supported on Clang and GCC.")
    endif()
endfunction()

enable_sanitizers(example)

# Enable testing
enable_testing()
