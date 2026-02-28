# SPDX-License-Identifier: MIT

set(_ORPHEUS_SANITIZERS_SUPPORTED OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(_ORPHEUS_SANITIZERS_SUPPORTED ON)
endif()

option(ORP_ENABLE_ASAN "Enable AddressSanitizer in Debug builds" ${_ORPHEUS_SANITIZERS_SUPPORTED})
option(ORP_ENABLE_UBSAN "Enable Undefined Behavior Sanitizer in Debug builds"
       ${_ORPHEUS_SANITIZERS_SUPPORTED})

add_library(orpheus_sanitizers INTERFACE)
add_library(Orpheus::sanitizers ALIAS orpheus_sanitizers)

if(_ORPHEUS_SANITIZERS_SUPPORTED)
  target_compile_options(orpheus_sanitizers
    INTERFACE
      "$<$<AND:$<CONFIG:Debug>,$<BOOL:${ORP_ENABLE_ASAN}>>:-fsanitize=address>"
      "$<$<AND:$<CONFIG:Debug>,$<BOOL:${ORP_ENABLE_UBSAN}>>:-fsanitize=undefined>"
  )

  target_link_options(orpheus_sanitizers
    INTERFACE
      "$<$<AND:$<CONFIG:Debug>,$<BOOL:${ORP_ENABLE_ASAN}>>:-fsanitize=address>"
      "$<$<AND:$<CONFIG:Debug>,$<BOOL:${ORP_ENABLE_UBSAN}>>:-fsanitize=undefined>"
  )

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(orpheus_sanitizers
      INTERFACE
        "$<$<AND:$<CONFIG:Debug>,$<BOOL:${ORP_ENABLE_UBSAN}>>:ubsan>"
    )
  endif()
else()
  message(STATUS "Sanitizers are not enabled on this compiler.")
endif()

function(orpheus_enable_sanitizers target_name)
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR "Cannot enable sanitizers for unknown target: ${target_name}")
  endif()

  get_target_property(_orpheus_target_type "${target_name}" TYPE)
  if(_orpheus_target_type STREQUAL "INTERFACE_LIBRARY")
    target_link_libraries("${target_name}" INTERFACE orpheus_sanitizers)
  else()
    target_link_libraries("${target_name}" PUBLIC orpheus_sanitizers)
  endif()
endfunction()
