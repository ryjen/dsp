cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED LIBDAISY_DIR)
    message(FATAL_ERROR "LIBDAISY_DIR is required")
endif()

project(dsp_hothouse_firmware LANGUAGES C CXX ASM)

set(CMAKE_CXX_EXTENSIONS OFF)
set(DSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
list(APPEND CMAKE_MODULE_PATH "${LIBDAISY_DIR}/cmake")

add_subdirectory("${LIBDAISY_DIR}" "${CMAKE_BINARY_DIR}/libdaisy")

add_library(dsp_embedded_core INTERFACE)
target_include_directories(dsp_embedded_core INTERFACE
    "${DSP_ROOT}/core/include"
)
target_compile_features(dsp_embedded_core INTERFACE cxx_std_20)

add_library(dsp_embedded_delay STATIC
    "${DSP_ROOT}/effects/delay/src/delay.cpp"
)
target_include_directories(dsp_embedded_delay PUBLIC
    "${DSP_ROOT}/effects/delay/include"
)
target_link_libraries(dsp_embedded_delay PUBLIC dsp_embedded_core)
target_compile_features(dsp_embedded_delay PUBLIC cxx_std_20)

add_library(dsp_embedded_control STATIC
    src/control_mapper.cpp
    src/delay_control_binding.cpp
    src/midi_adapter.cpp
    src/pedal_runtime.cpp
    src/tempo_controller.cpp
)
target_include_directories(dsp_embedded_control PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(dsp_embedded_control PUBLIC
    dsp_embedded_core
    dsp_embedded_delay
)
target_compile_features(dsp_embedded_control PUBLIC cxx_std_20)

set(FIRMWARE_NAME hothouse_delay)
set(FIRMWARE_SOURCES
    firmware/hothouse_main.cpp
    firmware/hothouse_hardware.cpp
    firmware/libdaisy_midi_bridge.cpp
)
set(DAISY_GENERATE_BIN ON)
include(DaisyProject)

target_include_directories(hothouse_delay PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware"
    "${DSP_ROOT}/core/include"
    "${DSP_ROOT}/effects/delay/include"
)
target_link_libraries(hothouse_delay PRIVATE
    dsp_embedded_control
    dsp_embedded_delay
)
set_target_properties(hothouse_delay PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
