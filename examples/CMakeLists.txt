add_executable(hello_fpe hello_fpe.cpp)
target_link_libraries(hello_fpe PRIVATE fpe::fpe)
fpe_configure_target(hello_fpe)
