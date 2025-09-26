add_executable(orpheus_conformance_json
  json_roundtrip.cpp
)

target_link_libraries(orpheus_conformance_json
  PRIVATE
    orpheus_core
    GTest::gtest_main
)

target_include_directories(orpheus_conformance_json
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/session
)

target_compile_definitions(orpheus_conformance_json
  PRIVATE
    ORPHEUS_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tools/fixtures"
)

include(GoogleTest)
set(TEST_NAME conformance_json)
add_test(NAME ${TEST_NAME} COMMAND orpheus_conformance_json)
