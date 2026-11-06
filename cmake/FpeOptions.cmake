# Warnings and sanitizers shared by every FPE target.

function(fpe_configure_target target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(FPE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
      -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Woverloaded-virtual)
    if(FPE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()

  if(FPE_SANITIZE)
    target_compile_options(${target} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
  endif()
  if(FPE_TSAN)
    target_compile_options(${target} PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(${target} PUBLIC -fsanitize=thread)
  endif()
endfunction()
