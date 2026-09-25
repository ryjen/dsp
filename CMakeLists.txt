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
target_compile_options(dsp_realtime_allocation_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)

add_library(dsp_native STATIC
    hosts/native/src/offline_renderer.cpp
    hosts/native/src/callback_budget.cpp
)
target_include_directories(dsp_native PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/hosts/native/include)
target_link_libraries(dsp_native PUBLIC dsp_core)

add_executable(dsp_offline_renderer_test test/native/offline_renderer_test.cpp)
target_link_libraries(dsp_offline_renderer_test PRIVATE dsp_native)
add_test(NAME dsp_offline_renderer_test COMMAND dsp_offline_renderer_test)

add_executable(dsp_callback_budget_test test/native/callback_budget_test.cpp)
target_link_libraries(dsp_callback_budget_test PRIVATE dsp_native)
add_test(NAME dsp_callback_budget_test COMMAND dsp_callback_budget_test)

add_library(dsp_tremolo STATIC effects/tremolo/src/tremolo.cpp)
target_include_directories(dsp_tremolo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/effects/tremolo/include)
target_include_directories(dsp_tremolo SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/effects/tremolo/generated)
target_link_libraries(dsp_tremolo PUBLIC dsp_core)

add_executable(dsp_tremolo_test test/effects/tremolo_test.cpp)
target_link_libraries(dsp_tremolo_test PRIVATE dsp_native dsp_tremolo)
add_test(NAME dsp_tremolo_test COMMAND dsp_tremolo_test)

add_executable(dsp_tremolo_realtime_test test/effects/tremolo_realtime_test.cpp)
target_link_libraries(dsp_tremolo_realtime_test PRIVATE dsp_native dsp_tremolo)
add_test(NAME dsp_tremolo_realtime_test COMMAND dsp_tremolo_realtime_test)
target_compile_options(dsp_tremolo_realtime_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)

add_executable(dsp_delay_time_test test/effects/delay_time_test.cpp)
target_include_directories(dsp_delay_time_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/effects/delay/include)
target_link_libraries(dsp_delay_time_test PRIVATE dsp_core)
add_test(NAME dsp_delay_time_test COMMAND dsp_delay_time_test)