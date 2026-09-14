cmake_minimum_required(VERSION 3.25)

project(dsp LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

add_library(dsp_core INTERFACE)
target_include_directories(dsp_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/core/include)
target_compile_features(dsp_core INTERFACE cxx_std_20)

enable_testing()

add_executable(dsp_native_smoke test/native/native_smoke.cpp)
target_link_libraries(dsp_native_smoke PRIVATE dsp_core)
add_test(NAME dsp_native_smoke COMMAND dsp_native_smoke)

add_executable(dsp_processor_contract_test test/core/processor_contract_test.cpp)
target_link_libraries(dsp_processor_contract_test PRIVATE dsp_core)
add_test(NAME dsp_processor_contract_test COMMAND dsp_processor_contract_test)

add_executable(dsp_control_primitives_test test/core/control_primitives_test.cpp)
target_link_libraries(dsp_control_primitives_test PRIVATE dsp_core)
add_test(NAME dsp_control_primitives_test COMMAND dsp_control_primitives_test)

add_executable(dsp_realtime_allocation_test test/core/realtime_allocation_test.cpp)
target_link_libraries(dsp_realtime_allocation_test PRIVATE dsp_core)
add_test(NAME dsp_realtime_allocation_test COMMAND dsp_realtime_allocation_test)

add_library(dsp_native STATIC
    hosts/native/src/offline_renderer.cpp
)
target_include_directories(dsp_native PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/hosts/native/include)
target_link_libraries(dsp_native PUBLIC dsp_core)

add_executable(dsp_offline_renderer_test test/native/offline_renderer_test.cpp)
target_link_libraries(dsp_offline_renderer_test PRIVATE dsp_native)
add_test(NAME dsp_offline_renderer_test COMMAND dsp_offline_renderer_test)
