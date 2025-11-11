# SPDX-License-Identifier: MIT

# Find libsndfile (optional, for audio file reading)
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SNDFILE sndfile)
endif()

if(NOT SNDFILE_FOUND)
    # Try find_library as fallback
    find_library(SNDFILE_LIBRARIES NAMES sndfile libsndfile)
    find_path(SNDFILE_INCLUDE_DIRS NAMES sndfile.h)

    if(SNDFILE_LIBRARIES AND SNDFILE_INCLUDE_DIRS)
        set(SNDFILE_FOUND TRUE)
    else()
        message(WARNING "libsndfile not found. Audio file reading will not be available.")
        message(WARNING "Install with: brew install libsndfile (macOS) or vcpkg install libsndfile (Windows)")
        set(SNDFILE_FOUND FALSE)
    endif()
endif()

# Build orpheus_audio_io with available components
set(ORPHEUS_AUDIO_IO_SOURCES
    dummy_audio_driver.cpp
)

if(SNDFILE_FOUND)
    list(APPEND ORPHEUS_AUDIO_IO_SOURCES audio_file_reader_libsndfile.cpp)
    list(APPEND ORPHEUS_AUDIO_IO_SOURCES waveform_processor.cpp)
    message(STATUS "✓ libsndfile FOUND - audio file reading enabled (with waveform processing)")
else()
    # Provide stub implementation when libsndfile is not available
    # This ensures createAudioFileReader() symbol exists for linking
    list(APPEND ORPHEUS_AUDIO_IO_SOURCES create_audio_file_reader_stub.cpp)
    message(STATUS "✗ libsndfile NOT FOUND - using stub implementation (audio file reading disabled)")
endif()

add_library(orpheus_audio_io STATIC
    ${ORPHEUS_AUDIO_IO_SOURCES}
)

target_compile_features(orpheus_audio_io PUBLIC cxx_std_20)

target_include_directories(orpheus_audio_io
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(orpheus_audio_io
    PUBLIC
        orpheus_session  # For SessionGraphError
)

if(SNDFILE_FOUND)
    target_include_directories(orpheus_audio_io PRIVATE ${SNDFILE_INCLUDE_DIRS})
    target_link_libraries(orpheus_audio_io PUBLIC ${SNDFILE_LIBRARIES})
    if(SNDFILE_LIBRARY_DIRS)
        target_link_directories(orpheus_audio_io PUBLIC ${SNDFILE_LIBRARY_DIRS})
    endif()
endif()

set_target_properties(orpheus_audio_io PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Apply compiler warnings
include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_audio_io)
