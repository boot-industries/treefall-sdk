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
    ${CMAKE_SOURCE_DIR}/adapters/shared
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

add_test(NAME orpheus_tests COMMAND orpheus_tests)
set_tests_properties(orpheus_tests PROPERTIES LABELS "fast;unit")

if(TARGET Orpheus::diagnostics)
  add_executable(verify_diagnostics_standalone
    verify_diagnostics_standalone.cpp
  )
  target_link_libraries(verify_diagnostics_standalone PRIVATE Orpheus::diagnostics)
  orpheus_enable_warnings(verify_diagnostics_standalone)
  add_test(NAME verify_diagnostics_standalone COMMAND verify_diagnostics_standalone)
endif()

if(TARGET Orpheus::audio_utils)
  add_executable(verify_audio_utils_standalone
    verify_audio_utils_standalone.cpp
  )
  target_link_libraries(verify_audio_utils_standalone PRIVATE Orpheus::audio_utils)
  orpheus_enable_warnings(verify_audio_utils_standalone)
  add_test(NAME verify_audio_utils_standalone COMMAND verify_audio_utils_standalone)
endif()

# M2: Real-time infrastructure tests
if(ORPHEUS_ENABLE_REALTIME)
  add_subdirectory(common)
  add_subdirectory(transport)
  if(TARGET orpheus_audio_io)
    add_subdirectory(audio_io)
  endif()
  if(TARGET orpheus_routing)
    add_subdirectory(routing)
  endif()
endif()

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  # ORP134 G1: the streaming-reader migration landed, so the audit runs as the
  # STRICT gate (ORP136 §2.1 / §4 "fail closed on realtime"): a reintroduced
  # audio-thread read is a hard failure, not a tracked-debt report.
  add_test(NAME realtime_static_audit
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/realtime_audit.py
      --root ${CMAKE_SOURCE_DIR}
      --fail-known-debt)

  # ORP133 G6: docs-path validation (broken internal links, references to the
  # extracted apps/clip-composer subdir, removed CMake options).
  add_test(NAME docs_path_audit
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/docs_path_audit.py
      --root ${CMAKE_SOURCE_DIR})

  add_test(NAME version_contract
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/version_contract.py
      --check)

  add_test(NAME shmui_juce_manifest
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/shmui_juce_manifest.py
      --check)
  add_test(NAME suite_manifest
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/suite.py
      validate
      --json)
  add_test(NAME suite_affected
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/suite.py
      affected
      shmui
      --json)
endif()

# Session tests (including Scene Manager)
add_subdirectory(session)

if(ORPHEUS_ENABLE_EXTENDED_TESTS)
set(FIND_PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests/cmake/find_package)
set(FIND_PACKAGE_BINARY_DIR ${CMAKE_BINARY_DIR}/tests/find_package_build)
set(FIND_PACKAGE_INSTALL_DIR ${CMAKE_BINARY_DIR}/tests/find_package_prefix)

add_test(NAME cmake_find_package
  COMMAND ${CMAKE_COMMAND}
    -Dsdk_build_dir=${CMAKE_BINARY_DIR}
    -Dinstall_prefix=${FIND_PACKAGE_INSTALL_DIR}
    -Drequired_version=${PROJECT_VERSION}
    -Dbuild_type=$<CONFIG>
    -Dsource_dir=${FIND_PACKAGE_SOURCE_DIR}
    -Dbinary_dir=${FIND_PACKAGE_BINARY_DIR}
    -P ${CMAKE_SOURCE_DIR}/tests/cmake/run_find_package.cmake)

set(PACKAGE_RUNTIME_SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests/package_runtime_consumer)
set(PACKAGE_RUNTIME_BINARY_DIR ${CMAKE_BINARY_DIR}/tests/package_runtime_consumer_build)
set(PACKAGE_RUNTIME_INSTALL_DIR ${CMAKE_BINARY_DIR}/tests/package_runtime_consumer_prefix)

add_test(NAME cmake_package_runtime_consumer
  COMMAND ${CMAKE_COMMAND}
    -Dsdk_build_dir=${CMAKE_BINARY_DIR}
    -Dinstall_prefix=${PACKAGE_RUNTIME_INSTALL_DIR}
    -Drequired_version=${PROJECT_VERSION}
    -Dbuild_type=$<CONFIG>
    -Dsource_dir=${PACKAGE_RUNTIME_SOURCE_DIR}
    -Dbinary_dir=${PACKAGE_RUNTIME_BINARY_DIR}
    -P ${CMAKE_SOURCE_DIR}/tests/cmake/run_find_package.cmake)

if(PROJECT_VERSION_MAJOR EQUAL 0 AND PROJECT_VERSION_MINOR GREATER 0)
  math(EXPR PREVIOUS_MINOR "${PROJECT_VERSION_MINOR} - 1")
  set(PREVIOUS_MINOR_VERSION
    "${PROJECT_VERSION_MAJOR}.${PREVIOUS_MINOR}.${PROJECT_VERSION_PATCH}")
  set(PACKAGE_REJECTION_BINARY_DIR
    ${CMAKE_BINARY_DIR}/tests/package_runtime_consumer_rejection_build)
  set(PACKAGE_REJECTION_INSTALL_DIR
    ${CMAKE_BINARY_DIR}/tests/package_runtime_consumer_rejection_prefix)

  add_test(NAME cmake_package_rejects_previous_minor
    COMMAND ${CMAKE_COMMAND}
      -Dsdk_build_dir=${CMAKE_BINARY_DIR}
      -Dinstall_prefix=${PACKAGE_REJECTION_INSTALL_DIR}
      -Drequired_version=${PREVIOUS_MINOR_VERSION}
      -Dbuild_type=$<CONFIG>
      -Dsource_dir=${PACKAGE_RUNTIME_SOURCE_DIR}
      -Dbinary_dir=${PACKAGE_REJECTION_BINARY_DIR}
      -Dexpect_configure_failure=ON
      -P ${CMAKE_SOURCE_DIR}/tests/cmake/run_find_package.cmake)
endif()

# ORP127 T10: submodule (add_subdirectory) external-consumer smoke test. Covers
# the co-development consumption path (FourTrack's choice) whose failure mode
# differs from find_package — missing add_subdirectory target/alias visibility
# rather than missing install()/export rules.
set(ADD_SUBDIR_SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests/cmake/add_subdirectory)
set(ADD_SUBDIR_BINARY_DIR ${CMAKE_BINARY_DIR}/tests/add_subdirectory_build)

add_test(NAME cmake_add_subdirectory
  COMMAND ${CMAKE_COMMAND}
    -Dbuild_type=$<CONFIG>
    -Dsource_dir=${ADD_SUBDIR_SOURCE_DIR}
    -Dbinary_dir=${ADD_SUBDIR_BINARY_DIR}
    -Dsdk_source_dir=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tests/cmake/run_add_subdirectory.cmake)

# ORP157: build the optional ShmUI package with its intended JUCE dependency,
# install it to an isolated prefix, then consume it as FreqFinder does: no SDK
# or ShmUI source-tree path, non-OpenGL only, and application-provided JUCE.
set(SHMUI_PACKAGE_PRODUCER_SOURCE_DIR
  ${CMAKE_SOURCE_DIR}/tests/cmake/shmui_package_producer)
set(SHMUI_PACKAGE_PRODUCER_BINARY_DIR
  ${CMAKE_BINARY_DIR}/tests/shmui_package_producer_build)
set(SHMUI_PACKAGE_CONSUMER_SOURCE_DIR
  ${CMAKE_SOURCE_DIR}/tests/cmake/shmui_package_consumer)
set(SHMUI_PACKAGE_CONSUMER_BINARY_DIR
  ${CMAKE_BINARY_DIR}/tests/shmui_package_consumer_build)
set(SHMUI_PACKAGE_INSTALL_DIR
  ${CMAKE_BINARY_DIR}/tests/shmui_package_prefix)
set(SHMUI_PACKAGE_DEPENDENCY_DIR
  ${CMAKE_BINARY_DIR}/tests/shmui_package_dependencies)

add_test(NAME cmake_shmui_package_consumer
  COMMAND ${CMAKE_COMMAND}
    -Dbuild_type=$<CONFIG>
    -Dsdk_source_dir=${CMAKE_SOURCE_DIR}
    -Dproducer_source_dir=${SHMUI_PACKAGE_PRODUCER_SOURCE_DIR}
    -Dproducer_binary_dir=${SHMUI_PACKAGE_PRODUCER_BINARY_DIR}
    -Dconsumer_source_dir=${SHMUI_PACKAGE_CONSUMER_SOURCE_DIR}
    -Dconsumer_binary_dir=${SHMUI_PACKAGE_CONSUMER_BINARY_DIR}
    -Dinstall_prefix=${SHMUI_PACKAGE_INSTALL_DIR}
    -Ddependency_dir=${SHMUI_PACKAGE_DEPENDENCY_DIR}
    -Drequired_version=${PROJECT_VERSION}
    -P ${CMAKE_SOURCE_DIR}/tests/cmake/run_shmui_package.cmake)
set_tests_properties(cmake_shmui_package_consumer PROPERTIES TIMEOUT 5400)

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
set_tests_properties(
  cmake_find_package
  cmake_package_runtime_consumer
  cmake_package_rejects_previous_minor
  cmake_add_subdirectory
  cmake_shmui_package_consumer
  abi_link
  PROPERTIES LABELS "extended;package")

# On Linux with GCC + ASan, dlopen-ing sanitized shared libraries from the
# (unsanitized) abi_link host aborts with "ASan runtime does not come first in
# initial library list" unless the runtime is preloaded. Mirror of the macOS
# DYLD_INSERT_LIBRARIES block below.
if(UNIX
   AND NOT APPLE
   AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_BUILD_TYPE MATCHES "Debug"
   AND ORP_ENABLE_ASAN)
  execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libasan.so
    OUTPUT_VARIABLE ASAN_SO_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(EXISTS "${ASAN_SO_PATH}")
    set_tests_properties(abi_link PROPERTIES
      ENVIRONMENT "LD_PRELOAD=${ASAN_SO_PATH}"
    )
  endif()
endif()

# On macOS with ASan + dlopen, interceptors fail unless the ASan dylib is
# preloaded via DYLD_INSERT_LIBRARIES. Detect the runtime path from the
# compiler and set it for the abi_link test (which uses dlopen internally).
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_BUILD_TYPE MATCHES "Debug")
  execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libclang_rt.asan_osx_dynamic.dylib
    OUTPUT_VARIABLE ASAN_DYLIB_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(EXISTS "${ASAN_DYLIB_PATH}")
    set_tests_properties(abi_link PROPERTIES
      ENVIRONMENT "DYLD_INSERT_LIBRARIES=${ASAN_DYLIB_PATH}"
    )
  endif()
endif()

endif()