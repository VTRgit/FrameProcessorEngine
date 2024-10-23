# Add the library
add_library(FrameProcessorEngineLib FrameProcessorEngine.cpp)

# Include directories
target_include_directories(FrameProcessorEngineLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
