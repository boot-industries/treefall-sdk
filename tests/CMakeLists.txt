# SPDX-License-Identifier: MIT
set(ORPHEUS_TEST_SOURCES
  abi_smoke.cpp
  adm_entity_graph.cpp
  render_tracks.cpp
  render_tracks_basic.cpp
  json_minhost_bridge_tests.cpp
  session_graph_ownership.cpp
  session_graph_invariants.cpp
  session_graph_scenes.cpp
  session_roundtrip.cpp
  session_api.cpp
  filename_rules.cpp
  otio_reconform_plan.cpp
  dsp/oscillator_test.cpp
)

if(TARGET reaper_orpheus)
  list(APPEND ORPHEUS_TEST_SOURCES reaper_adapter.cpp)
endif()

add_executable(orpheus_tests ${ORPHEUS_TEST_SOURCES})

target_link_libraries(orpheus_tests
  PRIVATE
    Orpheus::core
    GTest::gtest_main
)

if(ORPHEUS_THREADS_TARGET)
  target_link_libraries(orpheus_tests PRIVATE ${ORPHEUS_THREADS_TARGET})
endif()

target_compile_definitions(orpheus_tests
  PRIVATE
    ORPHEUS_SESSION_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/session"
)

if(TARGET reaper_orpheus)
  target_link_libraries(orpheus_tests PRIVATE reaper_orpheus)
endif()

if(ORP_EXTRA_UBSAN_LIB)
  target_link_libraries(orpheus_tests PRIVATE ${ORP_EXTRA_UBSAN_LIB})
endif()

target_include_directories(orpheus_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/session
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/adapters/reaper/include
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_tests)

if(ORP_BUILD_SHARED_CORE)
  add_custom_command(TARGET orpheus_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:orpheus_session> $<TARGET_FILE_DIR:orpheus_tests>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:orpheus_clipgrid> $<TARGET_FILE_DIR:orpheus_tests>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:orpheus_render> $<TARGET_FILE_DIR:orpheus_tests>
  )
  if(TARGET reaper_orpheus)
    add_custom_command(TARGET orpheus_tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_FILE:reaper_orpheus> $<TARGET_FILE_DIR:orpheus_tests>
    )
  endif()
endif()

include(GoogleTest)
gtest_discover_tests(orpheus_tests)

# M2: Real-time infrastructure tests
if(ORPHEUS_ENABLE_REALTIME)
  add_subdirectory(transport)
  if(TARGET orpheus_audio_io)
    add_subdirectory(audio_io)
  endif()
  if(TARGET orpheus_routing)
    add_subdirectory(routing)
  endif()
endif()

set(FIND_PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests/cmake/find_package)
set(FIND_PACKAGE_BINARY_DIR ${CMAKE_BINARY_DIR}/tests/find_package_build)

add_test(NAME cmake_find_package
  COMMAND ${CMAKE_COMMAND}
    -DOrpheusSDK_DIR=${CMAKE_BINARY_DIR}
    -Dsource_dir=${FIND_PACKAGE_SOURCE_DIR}
    -Dbinary_dir=${FIND_PACKAGE_BINARY_DIR}
    -P ${CMAKE_SOURCE_DIR}/tests/cmake/run_find_package.cmake)

add_executable(orpheus_abi_link
  abi_link/main.cpp
)

target_compile_definitions(orpheus_abi_link
  PRIVATE
    ORPHEUS_ABI_LINK_DIR="$<TARGET_FILE_DIR:orpheus_session>"
    ORPHEUS_SESSION_LIB="$<TARGET_FILE_NAME:orpheus_session>"
    ORPHEUS_CLIPGRID_LIB="$<TARGET_FILE_NAME:orpheus_clipgrid>"
    ORPHEUS_RENDER_LIB="$<TARGET_FILE_NAME:orpheus_render>"
    ORP_BUILD_SHARED_CORE=$<IF:$<BOOL:ORP_BUILD_SHARED_CORE>,1,0>
  )

target_link_libraries(orpheus_abi_link
  PRIVATE
    ${CMAKE_DL_LIBS}
)

target_include_directories(orpheus_abi_link
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

orpheus_enable_warnings(orpheus_abi_link)

add_test(NAME abi_link COMMAND orpheus_abi_link)

if(NOT ORP_BUILD_SHARED_CORE)
  set_tests_properties(abi_link PROPERTIES DISABLED ON)
endif()
