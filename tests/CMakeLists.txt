add_executable(orpheus_tests
  abi_smoke.cpp
  session_roundtrip.cpp
)

target_link_libraries(orpheus_tests
  PRIVATE
    orpheus_core
    GTest::gtest_main
)

target_include_directories(orpheus_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_tests)

include(GoogleTest)
gtest_discover_tests(orpheus_tests)
