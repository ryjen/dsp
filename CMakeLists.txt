cmake_minimum_required(VERSION 3.25)

project(dsp LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

option(
    DSP_ENABLE_JUCE_HOST
    "Build the optional JUCE desktop/audio-source host adapter"
    OFF
)

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
add_library(dsp_delay STATIC effects/delay/src/delay.cpp)
target_include_directories(dsp_delay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/effects/delay/include)
target_link_libraries(dsp_delay PUBLIC dsp_core)

add_executable(dsp_delay_test test/effects/delay_test.cpp)
target_link_libraries(dsp_delay_test PRIVATE dsp_native dsp_delay)
add_test(NAME dsp_delay_test COMMAND dsp_delay_test)

add_executable(dsp_delay_realtime_test test/effects/delay_realtime_test.cpp)
target_link_libraries(dsp_delay_realtime_test PRIVATE dsp_native dsp_delay)
add_test(NAME dsp_delay_realtime_test COMMAND dsp_delay_realtime_test)
target_compile_options(dsp_delay_realtime_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)



add_library(dsp_analysis STATIC
    analysis/src/performance_analyzer.cpp
)
target_include_directories(
    dsp_analysis
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/analysis/include
)
target_link_libraries(dsp_analysis PUBLIC dsp_core)

add_executable(
    dsp_performance_observation_queue_test
    test/analysis/performance_observation_queue_test.cpp
)
target_link_libraries(
    dsp_performance_observation_queue_test
    PRIVATE dsp_analysis
)
add_test(
    NAME dsp_performance_observation_queue_test
    COMMAND dsp_performance_observation_queue_test
)
target_compile_options(
    dsp_performance_observation_queue_test
    PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>
)

add_executable(
    dsp_performance_analyzer_test
    test/analysis/performance_analyzer_test.cpp
)
target_link_libraries(
    dsp_performance_analyzer_test
    PRIVATE dsp_analysis dsp_native
)
add_test(
    NAME dsp_performance_analyzer_test
    COMMAND dsp_performance_analyzer_test
)
target_compile_options(
    dsp_performance_analyzer_test
    PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>
)

add_executable(dsp_pedal_control_state_test test/control/pedal_control_state_test.cpp)
target_link_libraries(dsp_pedal_control_state_test PRIVATE dsp_core)
add_test(NAME dsp_pedal_control_state_test COMMAND dsp_pedal_control_state_test)
target_compile_options(dsp_pedal_control_state_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)


add_library(dsp_daisy_control STATIC
    targets/daisy/src/tempo_controller.cpp
    targets/daisy/src/midi_adapter.cpp
    targets/daisy/src/control_mapper.cpp
)
target_include_directories(dsp_daisy_control PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/targets/daisy/include)
target_link_libraries(dsp_daisy_control PUBLIC dsp_core)

add_executable(dsp_tempo_controller_test test/targets/daisy/tempo_controller_test.cpp)
target_link_libraries(dsp_tempo_controller_test PRIVATE dsp_daisy_control)
add_test(NAME dsp_tempo_controller_test COMMAND dsp_tempo_controller_test)
target_compile_options(dsp_tempo_controller_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)

add_executable(dsp_midi_adapter_test test/targets/daisy/midi_adapter_test.cpp)
target_link_libraries(dsp_midi_adapter_test PRIVATE dsp_daisy_control)
add_test(NAME dsp_midi_adapter_test COMMAND dsp_midi_adapter_test)
target_compile_options(dsp_midi_adapter_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)


add_library(dsp_daisy_delay_binding STATIC
    targets/daisy/src/delay_control_binding.cpp
)
target_include_directories(dsp_daisy_delay_binding PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/targets/daisy/include)
target_link_libraries(dsp_daisy_delay_binding PUBLIC dsp_daisy_control dsp_delay)

add_executable(dsp_control_mapper_test test/targets/daisy/control_mapper_test.cpp)
target_link_libraries(dsp_control_mapper_test PRIVATE dsp_daisy_delay_binding)
add_test(NAME dsp_control_mapper_test COMMAND dsp_control_mapper_test)
target_compile_options(dsp_control_mapper_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)


add_library(dsp_daisy_runtime STATIC
    targets/daisy/src/pedal_runtime.cpp
)
target_include_directories(dsp_daisy_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/targets/daisy/include)
target_link_libraries(dsp_daisy_runtime PUBLIC dsp_core)

add_executable(dsp_pedal_runtime_test test/targets/daisy/pedal_runtime_test.cpp)
target_link_libraries(dsp_pedal_runtime_test PRIVATE dsp_daisy_runtime dsp_delay)
add_test(NAME dsp_pedal_runtime_test COMMAND dsp_pedal_runtime_test)

add_executable(dsp_pedal_runtime_realtime_test test/targets/daisy/pedal_runtime_realtime_test.cpp)
target_link_libraries(dsp_pedal_runtime_realtime_test PRIVATE dsp_daisy_runtime dsp_delay)
add_test(NAME dsp_pedal_runtime_realtime_test COMMAND dsp_pedal_runtime_realtime_test)
target_compile_options(dsp_pedal_runtime_realtime_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)


add_executable(dsp_delay_external_storage_test test/effects/delay_external_storage_test.cpp)
target_link_libraries(dsp_delay_external_storage_test PRIVATE dsp_delay)
add_test(NAME dsp_delay_external_storage_test COMMAND dsp_delay_external_storage_test)
target_compile_options(dsp_delay_external_storage_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)


add_executable(dsp_realtime_midi_inbox_test test/targets/daisy/realtime_midi_inbox_test.cpp)
target_link_libraries(dsp_realtime_midi_inbox_test PRIVATE dsp_daisy_control)
add_test(NAME dsp_realtime_midi_inbox_test COMMAND dsp_realtime_midi_inbox_test)
target_compile_options(dsp_realtime_midi_inbox_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)


add_library(dsp_guitar_dsp_protocol STATIC
    protocols/src/guitar_dsp_preset_v1.cpp
)
target_include_directories(dsp_guitar_dsp_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/protocols/include)
target_compile_features(dsp_guitar_dsp_protocol PUBLIC cxx_std_20)

add_executable(dsp_guitar_dsp_preset_v1_test test/protocols/guitar_dsp_preset_v1_test.cpp)
target_link_libraries(dsp_guitar_dsp_preset_v1_test PRIVATE dsp_guitar_dsp_protocol)
add_test(NAME dsp_guitar_dsp_preset_v1_test COMMAND dsp_guitar_dsp_preset_v1_test)
target_compile_options(dsp_guitar_dsp_preset_v1_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)


add_library(dsp_delay_preset_binding STATIC
    protocols/src/delay_preset_binding.cpp
)
target_include_directories(dsp_delay_preset_binding PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/protocols/include)
target_link_libraries(dsp_delay_preset_binding PUBLIC dsp_guitar_dsp_protocol dsp_delay)

add_executable(dsp_delay_preset_binding_test test/protocols/delay_preset_binding_test.cpp)
target_link_libraries(dsp_delay_preset_binding_test PRIVATE dsp_delay_preset_binding)
add_test(NAME dsp_delay_preset_binding_test COMMAND dsp_delay_preset_binding_test)

# Deterministic sound demo: synthetic plucked strings -> existing portable effects.
add_executable(dsp_effects_audio_demo examples/audio_demo.cpp)
target_link_libraries(dsp_effects_audio_demo PRIVATE dsp_native dsp_tremolo dsp_delay)
add_test(NAME dsp_effects_audio_demo_test
    COMMAND dsp_effects_audio_demo "${CMAKE_CURRENT_BINARY_DIR}/demo-smoke")
# Offline execution plane used by guitarctl, with no dependency on its Python code.
add_executable(dsp_render_cli hosts/native/src/render_cli.cpp)
set_target_properties(dsp_render_cli PROPERTIES OUTPUT_NAME dsp)
target_link_libraries(dsp_render_cli PRIVATE
    dsp_native dsp_delay dsp_tremolo dsp_delay_preset_binding)
install(TARGETS dsp_render_cli RUNTIME DESTINATION bin)
add_test(NAME dsp_render_cli_test COMMAND ${CMAKE_COMMAND}
    "-DDEMO=$<TARGET_FILE:dsp_effects_audio_demo>"
    "-DCLI=$<TARGET_FILE:dsp_render_cli>"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/render-cli-smoke"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test/native/render_cli_smoke.cmake")



if(DSP_ENABLE_JUCE_HOST)
    enable_language(C)
    find_package(JUCE CONFIG REQUIRED)

    add_library(dsp_juce_host STATIC
        hosts/juce/src/processor_audio_source.cpp
    )
    target_include_directories(
        dsp_juce_host
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/hosts/juce/include
    )
    target_link_libraries(
        dsp_juce_host
        PUBLIC dsp_core juce::juce_audio_basics
    )
    target_compile_definitions(
        dsp_juce_host
        PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    )

    add_executable(
        dsp_juce_host_test
        test/hosts/juce/processor_audio_source_test.cpp
    )
    target_link_libraries(
        dsp_juce_host_test
        PRIVATE dsp_juce_host dsp_tremolo
    )
    add_test(NAME dsp_juce_host_test COMMAND dsp_juce_host_test)
    target_compile_options(
        dsp_juce_host_test
        PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>
    )
endif()


option(DSP_ENABLE_LV2_HOST "Build optional stereo LV2 plugins" OFF)
if(DSP_ENABLE_LV2_HOST)
    include(GNUInstallDirs)
    find_path(LV2_INCLUDE_DIR NAMES lv2/core/lv2.h REQUIRED)
    set_target_properties(dsp_delay dsp_tremolo PROPERTIES POSITION_INDEPENDENT_CODE ON)
    foreach(effect IN ITEMS delay tremolo)
        string(TOUPPER "${effect}" effect_upper)
        set(bundle "${CMAKE_CURRENT_BINARY_DIR}/lv2/micrantha-${effect}.lv2")
        file(MAKE_DIRECTORY "${bundle}")
        foreach(metadata IN ITEMS manifest.ttl "${effect}.ttl")
            configure_file("hosts/lv2/metadata/${effect}/${metadata}" "${bundle}/${metadata}" COPYONLY)
        endforeach()
        add_library(dsp_lv2_${effect} MODULE hosts/lv2/src/plugin.cpp)
        target_compile_definitions(dsp_lv2_${effect} PRIVATE DSP_LV2_${effect_upper}=1)
        target_include_directories(dsp_lv2_${effect} SYSTEM PRIVATE "${LV2_INCLUDE_DIR}")
        target_link_libraries(dsp_lv2_${effect} PRIVATE dsp_${effect})
        set_target_properties(dsp_lv2_${effect} PROPERTIES PREFIX "" OUTPUT_NAME "micrantha-${effect}" LIBRARY_OUTPUT_DIRECTORY "${bundle}")
        install(TARGETS dsp_lv2_${effect} LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/lv2/micrantha-${effect}.lv2")
        install(FILES "hosts/lv2/metadata/${effect}/manifest.ttl" "hosts/lv2/metadata/${effect}/${effect}.ttl"
                DESTINATION "${CMAKE_INSTALL_LIBDIR}/lv2/micrantha-${effect}.lv2")
    endforeach()
    add_executable(dsp_lv2_host_test test/hosts/lv2/lv2_host_test.cpp)
    target_include_directories(dsp_lv2_host_test SYSTEM PRIVATE "${LV2_INCLUDE_DIR}")
    target_link_libraries(dsp_lv2_host_test PRIVATE "${CMAKE_DL_LIBS}")
    # Export allocation instrumentation for ELF symbol interposition when
    # dlopen() loads plugin .so. This is a test-only setting.
    set_target_properties(dsp_lv2_host_test PROPERTIES ENABLE_EXPORTS ON)
    target_compile_options(dsp_lv2_host_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-mismatched-new-delete>)
    foreach(effect IN ITEMS delay tremolo)
        add_test(NAME dsp_lv2_${effect}_host_test
            COMMAND dsp_lv2_host_test "$<TARGET_FILE:dsp_lv2_${effect}>" "https://micrantha.com/lv2/${effect}" "${effect}")
    endforeach()
endif()
