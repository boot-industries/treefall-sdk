# SPDX-License-Identifier: MIT

# Unit tests
add_executable(transport_controller_test
    transport_controller_test.cpp
)

target_link_libraries(transport_controller_test
    PRIVATE
        orpheus_transport
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(transport_controller_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME transport_controller_test
    COMMAND transport_controller_test
)

# Integration tests (requires orpheus_audio_io)
if(TARGET orpheus_audio_io)
    # Find libsndfile (needed because orpheus_audio_io is static)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SNDFILE sndfile)
    endif()

    if(NOT SNDFILE_FOUND)
        find_library(SNDFILE_LIBRARIES NAMES sndfile libsndfile)
        find_path(SNDFILE_INCLUDE_DIRS NAMES sndfile.h)
        if(SNDFILE_LIBRARIES AND SNDFILE_INCLUDE_DIRS)
            set(SNDFILE_FOUND TRUE)
        endif()
    endif()

    add_executable(transport_integration_test
        integration_test.cpp
    )

    target_link_libraries(transport_integration_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found (needed because orpheus_audio_io is static)
    if(SNDFILE_FOUND)
        target_link_libraries(transport_integration_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(transport_integration_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(transport_integration_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME transport_integration_test
        COMMAND transport_integration_test
    )

    # Multi-clip stress test
    add_executable(multi_clip_stress_test
        multi_clip_stress_test.cpp
    )

    target_link_libraries(multi_clip_stress_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found (needed because orpheus_audio_io is static)
    if(SNDFILE_FOUND)
        target_link_libraries(multi_clip_stress_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(multi_clip_stress_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(multi_clip_stress_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME multi_clip_stress_test
        COMMAND multi_clip_stress_test
    )
endif()
