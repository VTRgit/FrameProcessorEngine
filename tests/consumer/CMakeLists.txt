# Checks that FPE is consumable with add_subdirectory as fpe::fpe without its tests.
cmake_minimum_required(VERSION 3.21)
project(fpe_consumer LANGUAGES CXX)
add_subdirectory(../.. fpe-build)
if(TARGET fpe_tests)
  message(FATAL_ERROR "FPE tests must not be built when FPE is a subproject")
endif()
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE fpe::fpe)
