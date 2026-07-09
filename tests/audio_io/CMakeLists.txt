# SPDX-License-Identifier: MIT

# ORP127 T8 (G6): polyphase resampler unit test. Standalone — links only
# orpheus_audio_utils (no libsndfile, no JUCE, host-neutral).
add_executable(polyphase_resampler_test
    polyphase_resampler_test.cpp
)

target_link_libraries(polyphase_resampler_test
    PRIVATE
        orpheus_audio_utils
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(polyphase_resampler_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME polyphase_resampler_test COMMAND polyphase_resampler_test)

# ORP128/FTR018 §2: scrub resampler unit test. Standalone — links only
# orpheus_audio_utils (no libsndfile, no JUCE, host-neutral).
add_executable(scrub_resampler_test
    scrub_resampler_test.cpp
)

target_link_libraries(scrub_resampler_test
    PRIVATE
        orpheus_audio_utils
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(scrub_resampler_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME scrub_resampler_test COMMAND scrub_resampler_test)

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

# ORP128/FTR018 §1: backward/windowed read (readSamplesEndingAt). The base-default
# half (in-memory fake reader) is host-neutral; the libsndfile-override half is
# compiled in only when sndfile is present (guarded by ORPHEUS_HAVE_SNDFILE).
add_executable(reverse_read_test
    reverse_read_test.cpp
)

target_link_libraries(reverse_read_test
    PRIVATE
        orpheus_audio_utils
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(reverse_read_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

if(SNDFILE_FOUND)
    target_compile_definitions(reverse_read_test PRIVATE ORPHEUS_HAVE_SNDFILE)
    target_include_directories(reverse_read_test PRIVATE ${SNDFILE_INCLUDE_DIRS})
    target_link_libraries(reverse_read_test PRIVATE ${SNDFILE_LIBRARIES})
    if(SNDFILE_LIBRARY_DIRS)
        target_link_directories(reverse_read_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
    endif()
endif()

add_test(NAME reverse_read_test COMMAND reverse_read_test)

# Audio file reader tests (requires libsndfile)
if(SNDFILE_FOUND)
    add_executable(audio_file_reader_test
        audio_file_reader_test.cpp
    )

    target_link_libraries(audio_file_reader_test
        PRIVATE
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
            ${SNDFILE_LIBRARIES}
    )

    if(SNDFILE_LIBRARY_DIRS)
        target_link_directories(audio_file_reader_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
    endif()

    target_include_directories(audio_file_reader_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(NAME audio_file_reader_test COMMAND audio_file_reader_test)

    # ORP134 G5 / FTR007: IAudioFileWriter round-trip tests (requires libsndfile)
    add_executable(audio_file_writer_test
        audio_file_writer_test.cpp
    )

    target_link_libraries(audio_file_writer_test
        PRIVATE
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
            ${SNDFILE_LIBRARIES}
    )

    if(SNDFILE_LIBRARY_DIRS)
        target_link_directories(audio_file_writer_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
    endif()

    target_include_directories(audio_file_writer_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/tests
    )

    add_test(NAME audio_file_writer_test COMMAND audio_file_writer_test)

    # Waveform processor tests (requires libsndfile)
    add_executable(waveform_processor_test
        waveform_processor_test.cpp
    )

    target_link_libraries(waveform_processor_test
        PRIVATE
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
            ${SNDFILE_LIBRARIES}
    )

    if(SNDFILE_LIBRARY_DIRS)
        target_link_directories(waveform_processor_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
    endif()

    target_include_directories(waveform_processor_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
            ${SNDFILE_INCLUDE_DIRS}
    )

    add_test(NAME waveform_processor_test COMMAND waveform_processor_test)
endif()

# Dummy driver tests
add_executable(dummy_driver_test
    dummy_driver_test.cpp
)

target_link_libraries(dummy_driver_test
    PRIVATE
        orpheus_audio_io
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

# If libsndfile is found, link it (needed because orpheus_audio_io is static)
if(SNDFILE_FOUND)
    target_link_libraries(dummy_driver_test PRIVATE ${SNDFILE_LIBRARIES})
    if(SNDFILE_LIBRARY_DIRS)
        target_link_directories(dummy_driver_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
    endif()
endif()

target_include_directories(dummy_driver_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(NAME dummy_driver_test COMMAND dummy_driver_test)

# CoreAudio driver tests (macOS only)
if(ORPHEUS_ENABLE_COREAUDIO)
    add_executable(coreaudio_driver_test
        coreaudio_driver_test.cpp
    )

    target_link_libraries(coreaudio_driver_test
        PRIVATE
            orpheus_audio_driver_coreaudio
            orpheus_session
            GTest::gtest
            GTest::gtest_main
            "-framework CoreFoundation"
    )

    target_include_directories(coreaudio_driver_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(NAME coreaudio_driver_test COMMAND coreaudio_driver_test)
endif()

# Audio Driver Manager tests
add_executable(driver_manager_test
    driver_manager_test.cpp
)

target_link_libraries(driver_manager_test
    PRIVATE
        orpheus_audio_driver_manager
        orpheus_audio_io  # For dummy driver
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

# If CoreAudio is enabled, link it for hot-swap tests
if(ORPHEUS_ENABLE_COREAUDIO)
    target_link_libraries(driver_manager_test PRIVATE
        orpheus_audio_driver_coreaudio
        "-framework CoreFoundation"
    )
endif()

# If libsndfile is found, link it (needed because orpheus_audio_io is static)
if(SNDFILE_FOUND)
    target_link_libraries(driver_manager_test PRIVATE ${SNDFILE_LIBRARIES})
    if(SNDFILE_LIBRARY_DIRS)
        target_link_directories(driver_manager_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
    endif()
endif()

target_include_directories(driver_manager_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(NAME driver_manager_test COMMAND driver_manager_test)

# Device hot-swap integration tests
add_executable(device_hot_swap_test
    device_hot_swap_test.cpp
)

target_link_libraries(device_hot_swap_test
    PRIVATE
        orpheus_audio_driver_manager
        orpheus_audio_io  # For dummy driver
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

# If CoreAudio is enabled, link it for hot-swap tests
if(ORPHEUS_ENABLE_COREAUDIO)
    target_link_libraries(device_hot_swap_test PRIVATE
        orpheus_audio_driver_coreaudio
        "-framework CoreFoundation"
    )
endif()

# If libsndfile is found, link it (needed because orpheus_audio_io is static)
if(SNDFILE_FOUND)
    target_link_libraries(device_hot_swap_test PRIVATE ${SNDFILE_LIBRARIES})
    if(SNDFILE_LIBRARY_DIRS)
        target_link_directories(device_hot_swap_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
    endif()
endif()

target_include_directories(device_hot_swap_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(NAME device_hot_swap_test COMMAND device_hot_swap_test)
