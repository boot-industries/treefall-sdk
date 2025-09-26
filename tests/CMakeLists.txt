add_executable(orpheus_tests
  abi_smoke.cpp
  session_roundtrip.cpp
)

target_link_libraries(orpheus_tests
  PRIVATE
    Orpheus::core
    GTest::gtest_main
)

if(ORP_EXTRA_UBSAN_LIB)
  target_link_libraries(orpheus_tests PRIVATE ${ORP_EXTRA_UBSAN_LIB})
endif()

target_include_directories(orpheus_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_tests)

include(GoogleTest)
gtest_discover_tests(orpheus_tests)

set(FIND_PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests/cmake/find_package)
set(FIND_PACKAGE_BINARY_DIR ${CMAKE_BINARY_DIR}/tests/find_package_build)

add_test(NAME cmake_find_package
  COMMAND ${CMAKE_COMMAND}
    -DOrpheusSDK_DIR=${CMAKE_BINARY_DIR}
    -Dsource_dir=${FIND_PACKAGE_SOURCE_DIR}
    -Dbinary_dir=${FIND_PACKAGE_BINARY_DIR}
    -P ${CMAKE_SOURCE_DIR}/tests/cmake/run_find_package.cmake)
