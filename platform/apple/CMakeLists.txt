# Apple capture adapter (F7). Built when FPE is configured for macOS or iOS.
# Consumers: add_subdirectory(FrameProcessorEngine) then link fpe::apple.

add_library(fpe_apple STATIC src/camera.cpp)
add_library(fpe::apple ALIAS fpe_apple)
target_include_directories(fpe_apple PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(fpe_apple PUBLIC fpe::fpe "-framework CoreVideo" "-framework CoreMedia"
                      "-framework CoreFoundation")
fpe_configure_target(fpe_apple)
