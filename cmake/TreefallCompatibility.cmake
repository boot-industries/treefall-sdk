# SPDX-License-Identifier: MIT
# One table maps the public facade to existing binaries. Optional targets remain absent.
function(_treefall_compatibility_target_table result)
set(${result}
  core|orpheus_core
  session|orpheus_session
  clipgrid|orpheus_clipgrid
  render|orpheus_render
  diagnostics|orpheus_diagnostics
  audio_utils|orpheus_audio_utils
  audio_io|orpheus_audio_io
  audio_driver_manager|orpheus_audio_driver_manager
  routing|orpheus_routing
  transport|orpheus_transport
  audio_driver_coreaudio|orpheus_audio_driver_coreaudio
  audio_driver_wasapi|orpheus_audio_driver_wasapi
  sanitizers|orpheus_sanitizers
  shmui_juce|orpheus_shmui_juce
  shmui_juce_gl|orpheus_shmui_juce_gl
  adapters_common|orpheus_adapters_common PARENT_SCOPE)
endfunction()
_treefall_compatibility_target_table(TREEFALL_COMPATIBILITY_TARGETS)

if(NOT COMMAND _orpheus_sdk_alias)
  function(_orpheus_sdk_alias stable_name exported_name)
    if(TARGET "${exported_name}" AND NOT TARGET "${stable_name}")
      get_target_property(_treefall_backing_imported "${exported_name}" IMPORTED)
      if(_treefall_backing_imported)
        add_library("${stable_name}" INTERFACE IMPORTED)
      else()
        # Source integrations consume aliases from the parent directory.
        add_library("${stable_name}" INTERFACE IMPORTED GLOBAL)
      endif()
      set_property(TARGET "${stable_name}" PROPERTY INTERFACE_LINK_LIBRARIES "${exported_name}")
    endif()
  endfunction()
endif()

# Call again after adding optional SDK-owned ShmUI targets in source integrations.
# No GLOBAL include guard: imported facades must be available in each discovery scope.
function(treefall_register_compatibility_targets)
  _treefall_compatibility_target_table(TREEFALL_COMPATIBILITY_TARGETS)
  foreach(entry IN LISTS TREEFALL_COMPATIBILITY_TARGETS)
    string(REPLACE "|" ";" pair "${entry}")
    list(GET pair 0 stable)
    list(GET pair 1 native)
    if(TARGET "Orpheus::${native}")
      _orpheus_sdk_alias("Treefall::${stable}" "Orpheus::${native}")
    elseif(TARGET "${native}")
      _orpheus_sdk_alias("Treefall::${stable}" "${native}")
    endif()
  endforeach()
endfunction()
