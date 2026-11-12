add_executable(hello_fpe hello_fpe.cpp)
target_link_libraries(hello_fpe PRIVATE fpe::fpe)
fpe_configure_target(hello_fpe)

# macOS camera demo (F7): Objective-C++ on AVFoundation, through fpe::apple.
if(APPLE AND NOT IOS)
  enable_language(OBJCXX)
  add_executable(camera_demo macos/camera_demo.mm)
  target_compile_options(camera_demo PRIVATE -fobjc-arc)
  target_link_libraries(camera_demo PRIVATE fpe::apple "-framework AVFoundation" "-framework Foundation")
endif()
