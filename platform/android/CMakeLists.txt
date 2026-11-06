# Android camera adapter. Built when FPE is configured for Android (the NDK toolchain sets ANDROID).
# Consumers: add_subdirectory(FrameProcessorEngine) then link fpe::android.

add_library(fpe_android STATIC src/camera.cpp)
add_library(fpe::android ALIAS fpe_android)
target_include_directories(fpe_android PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(fpe_android PUBLIC fpe::fpe mediandk nativewindow android)
fpe_configure_target(fpe_android)
