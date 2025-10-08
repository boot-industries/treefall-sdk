# SPDX-License-Identifier: MIT

function(orpheus_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4 /WX /permissive- /Zc:preprocessor /Zc:__cplusplus /EHsc
      /wd5105
      /external:W0 /external:anglebrackets /experimental:external
    )
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Werror -Wconversion -Wsign-conversion
    )
  endif()
endfunction()
