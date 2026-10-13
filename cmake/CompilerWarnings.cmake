# SPDX-License-Identifier: MIT

function(orpheus_enable_warnings target)
  if(MSVC)
    set(warnings
      /W4 /WX /permissive- /Zc:preprocessor /Zc:__cplusplus /EHsc
      /wd5105
      /external:W0 /external:anglebrackets /experimental:external
    )
  else()
    set(warnings -Wall -Wextra -Werror -Wconversion -Wsign-conversion)
  endif()
  if(ARGN)
    # JUCE contributes third-party translation units to the app target; keep
    # strict project warnings on explicitly owned sources, not those modules.
    set_property(SOURCE ${ARGN} APPEND PROPERTY COMPILE_OPTIONS ${warnings})
  else()
    target_compile_options(${target} PRIVATE ${warnings})
  endif()
endfunction()
