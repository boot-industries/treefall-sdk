cmake_minimum_required(VERSION 3.22)
project(orpheus_retired_enum_rejection LANGUAGES CXX)

find_package(OrpheusSDK CONFIG REQUIRED)
enable_testing()
add_executable(public_audio_io_consumer ../find_package/audio_io_telemetry.cpp)
target_compile_features(public_audio_io_consumer PRIVATE cxx_std_20)
target_link_libraries(public_audio_io_consumer PRIVATE Orpheus::audio_io)
if(WIN32)
  foreach(runtime_target
      Orpheus::orpheus_abi_runtime Orpheus::orpheus_session
      Orpheus::orpheus_clipgrid Orpheus::orpheus_render)
    if(TARGET ${runtime_target})
      add_custom_command(TARGET public_audio_io_consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:public_audio_io_consumer>
                $<TARGET_FILE:${runtime_target}>
                $<TARGET_FILE_DIR:public_audio_io_consumer>
        COMMAND_EXPAND_LISTS)
    endif()
  endforeach()
endif()
add_test(NAME public_audio_io_consumer COMMAND public_audio_io_consumer)

set(_retired_enum_name_prefix "AudioDriver")
set(_retired_enum_name_suffix "RuntimeOutcome")
string(CONCAT RETIRED_ENUM_NAME "${_retired_enum_name_prefix}" "${_retired_enum_name_suffix}")
configure_file(main.cpp.in "${CMAKE_CURRENT_BINARY_DIR}/main.cpp" @ONLY)
add_executable(retired_enum_consumer "${CMAKE_CURRENT_BINARY_DIR}/main.cpp")
target_compile_features(retired_enum_consumer PRIVATE cxx_std_20)
target_link_libraries(retired_enum_consumer PRIVATE Orpheus::audio_io)
