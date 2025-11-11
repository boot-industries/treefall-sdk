# SPDX-License-Identifier: MIT

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
