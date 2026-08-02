# SPDX-License-Identifier: MIT

# Scene Manager Tests
add_executable(scene_manager_test
  scene_manager_test.cpp
)

target_link_libraries(scene_manager_test
  PRIVATE
    Orpheus::core
    GTest::gtest_main
)

if(ORPHEUS_THREADS_TARGET)
  target_link_libraries(scene_manager_test PRIVATE ${ORPHEUS_THREADS_TARGET})
endif()

if(ORP_EXTRA_UBSAN_LIB)
  target_link_libraries(scene_manager_test PRIVATE ${ORP_EXTRA_UBSAN_LIB})
endif()

target_include_directories(scene_manager_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/session
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(scene_manager_test)

add_test(NAME scene_manager_test COMMAND scene_manager_test)
set_tests_properties(scene_manager_test PROPERTIES LABELS "fast;unit")

# ORP134 G2: identity & time-domain primitives (header-only; JSON round-trip)
add_executable(identity_time_test
  identity_time_test.cpp
)

target_link_libraries(identity_time_test
  PRIVATE
    Orpheus::core
    GTest::gtest_main
)

target_include_directories(identity_time_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

orpheus_enable_warnings(identity_time_test)

add_test(NAME identity_time_test COMMAND identity_time_test)

# ORP134 G8: scene manager <-> routing matrix wiring (needs orpheus_routing)
if(TARGET orpheus_routing)
  add_executable(scene_routing_test
    scene_routing_test.cpp
  )

  target_link_libraries(scene_routing_test
    PRIVATE
      Orpheus::core
      orpheus_routing
      GTest::gtest_main
  )

  target_include_directories(scene_routing_test
    PRIVATE
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/src/core
      ${CMAKE_SOURCE_DIR}/src/core/session
  )

  orpheus_enable_warnings(scene_routing_test)

  add_test(NAME scene_routing_test COMMAND scene_routing_test)
endif()
