include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1 # Specify the version or branch
)
FetchContent_MakeAvailable(spdlog)

# Add the library
add_library(FrameProcessorEngineLib
    frame_processor_engine_interface.cpp
    frame_processor_engine.cpp
    real_time_executor/real_time_executor.cpp
)

# Include directories
target_include_directories(FrameProcessorEngineLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(FrameProcessorEngineLib PRIVATE spdlog::spdlog)