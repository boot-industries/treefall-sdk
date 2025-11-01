# SPDX-License-Identifier: MIT

# Platform Audio Drivers
#
# Provides platform-specific audio I/O implementations:
# - CoreAudio (macOS)
# - WASAPI (Windows)
# - ASIO (Windows, optional)
# - Dummy (all platforms, for testing)

# Build options
option(ORPHEUS_ENABLE_COREAUDIO "Build CoreAudio driver (macOS)" ${APPLE})
option(ORPHEUS_ENABLE_WASAPI "Build WASAPI driver (Windows)" OFF)  # Not yet implemented
option(ORPHEUS_ENABLE_ASIO "Build ASIO driver (Windows, requires ASIO SDK)" OFF)

# Note: Dummy driver is built in core/audio_io alongside audio file reader

# CoreAudio driver (macOS)
if(ORPHEUS_ENABLE_COREAUDIO)
    add_library(orpheus_audio_driver_coreaudio STATIC
        coreaudio/coreaudio_driver.cpp
    )

    target_include_directories(orpheus_audio_driver_coreaudio
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src/core
    )

    target_link_libraries(orpheus_audio_driver_coreaudio
        PUBLIC
            orpheus_session  # For SessionGraphError
            "-framework CoreAudio"
            "-framework AudioToolbox"
    )

    target_compile_features(orpheus_audio_driver_coreaudio PUBLIC cxx_std_20)
    set_target_properties(orpheus_audio_driver_coreaudio PROPERTIES POSITION_INDEPENDENT_CODE ON)

    target_compile_definitions(orpheus_audio_driver_coreaudio
        PUBLIC
            ORPHEUS_ENABLE_COREAUDIO
    )

    orpheus_enable_warnings(orpheus_audio_driver_coreaudio)

    message(STATUS "Orpheus: CoreAudio driver enabled")
endif()

# WASAPI driver (Windows)
if(ORPHEUS_ENABLE_WASAPI)
    add_library(orpheus_audio_driver_wasapi STATIC
        wasapi/wasapi_driver.cpp
    )

    target_include_directories(orpheus_audio_driver_wasapi
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src/core
    )

    target_link_libraries(orpheus_audio_driver_wasapi
        PUBLIC
            orpheus_session  # For SessionGraphError
            Ole32
            Avrt
    )

    target_compile_features(orpheus_audio_driver_wasapi PUBLIC cxx_std_20)
    set_target_properties(orpheus_audio_driver_wasapi PROPERTIES POSITION_INDEPENDENT_CODE ON)

    target_compile_definitions(orpheus_audio_driver_wasapi
        PUBLIC
            ORPHEUS_ENABLE_WASAPI
    )

    orpheus_enable_warnings(orpheus_audio_driver_wasapi)

    message(STATUS "Orpheus: WASAPI driver enabled")
endif()

# ASIO driver (Windows, optional)
if(ORPHEUS_ENABLE_ASIO)
    if(NOT DEFINED ASIO_SDK_PATH)
        message(FATAL_ERROR "ASIO_SDK_PATH must be set when ORPHEUS_ENABLE_ASIO=ON")
    endif()

    add_library(orpheus_audio_driver_asio STATIC
        asio/asio_driver.cpp
    )

    target_include_directories(orpheus_audio_driver_asio
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src/core
            ${ASIO_SDK_PATH}/common
            ${ASIO_SDK_PATH}/host
            ${ASIO_SDK_PATH}/host/pc
    )

    target_link_libraries(orpheus_audio_driver_asio
        PUBLIC
            orpheus_session  # For SessionGraphError
    )

    target_compile_features(orpheus_audio_driver_asio PUBLIC cxx_std_20)
    set_target_properties(orpheus_audio_driver_asio PROPERTIES POSITION_INDEPENDENT_CODE ON)

    target_compile_definitions(orpheus_audio_driver_asio
        PUBLIC
            ORPHEUS_ENABLE_ASIO
    )

    orpheus_enable_warnings(orpheus_audio_driver_asio)

    message(STATUS "Orpheus: ASIO driver enabled (SDK path: ${ASIO_SDK_PATH})")
endif()

# Aggregate library (links all enabled drivers)
add_library(orpheus_audio_drivers INTERFACE)

target_link_libraries(orpheus_audio_drivers
    INTERFACE
        $<$<BOOL:${ORPHEUS_ENABLE_COREAUDIO}>:orpheus_audio_driver_coreaudio>
        $<$<BOOL:${ORPHEUS_ENABLE_WASAPI}>:orpheus_audio_driver_wasapi>
        $<$<BOOL:${ORPHEUS_ENABLE_ASIO}>:orpheus_audio_driver_asio>
)
