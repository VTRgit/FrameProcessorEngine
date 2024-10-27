# Add the library
add_library(FrameProcessorEngineLib
    frame_processor_engine_interface.cpp
    frame_processor_engine.cpp
    real_time_executor/real_time_executor.cpp
)

# Include directories
target_include_directories(FrameProcessorEngineLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
