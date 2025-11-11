# SPDX-License-Identifier: MIT

# Unit tests
add_executable(transport_controller_test
    transport_controller_test.cpp
)

target_link_libraries(transport_controller_test
    PRIVATE
        orpheus_transport
        orpheus_audio_io
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

# Fade processing tests
add_executable(fade_processing_test
    fade_processing_test.cpp
)

target_link_libraries(fade_processing_test
    PRIVATE
        orpheus_transport
        orpheus_audio_io
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(fade_processing_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME fade_processing_test
    COMMAND fade_processing_test
)

# OUT point enforcement tests (ORP091, ORP093)
add_executable(out_point_enforcement_test
    out_point_enforcement_test.cpp
)

target_link_libraries(out_point_enforcement_test
    PRIVATE
        orpheus_transport
        orpheus_audio_io
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(out_point_enforcement_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME out_point_enforcement_test
    COMMAND out_point_enforcement_test
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

    # Clip gain tests (ORP099 Task 1.1)
    add_executable(clip_gain_test
        clip_gain_test.cpp
    )

    target_link_libraries(clip_gain_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found (needed because orpheus_audio_io is static)
    if(SNDFILE_FOUND)
        target_link_libraries(clip_gain_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_gain_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_gain_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_gain_test
        COMMAND clip_gain_test
    )

    # Clip loop tests (ORP099 Task 1.2)
    add_executable(clip_loop_test
        clip_loop_test.cpp
    )

    target_link_libraries(clip_loop_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(clip_loop_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_loop_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_loop_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_loop_test
        COMMAND clip_loop_test
    )

    # Clip metadata tests (ORP099 Task 1.3)
    add_executable(clip_metadata_test
        clip_metadata_test.cpp
    )

    target_link_libraries(clip_metadata_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(clip_metadata_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_metadata_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_metadata_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_metadata_test
        COMMAND clip_metadata_test
    )

    # Clip restart tests
    add_executable(clip_restart_test
        clip_restart_test.cpp
    )

    target_link_libraries(clip_restart_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(clip_restart_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_restart_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_restart_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_restart_test
        COMMAND clip_restart_test
    )

    # Clip seek tests
    add_executable(clip_seek_test
        clip_seek_test.cpp
    )

    target_link_libraries(clip_seek_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(clip_seek_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_seek_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_seek_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_seek_test
        COMMAND clip_seek_test
    )

    # Clip cue points tests (ORP109 Feature 6)
    add_executable(clip_cue_points_test
        clip_cue_points_test.cpp
    )

    target_link_libraries(clip_cue_points_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(clip_cue_points_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_cue_points_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_cue_points_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/tests
    )

    add_test(
        NAME clip_cue_points_test
        COMMAND clip_cue_points_test
    )
endif()
