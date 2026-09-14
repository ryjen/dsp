cmake_minimum_required(VERSION 3.25)

project(dsp LANGUAGES CXX)

add_library(dsp_core INTERFACE)
target_include_directories(dsp_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/core/include)
target_compile_features(dsp_core INTERFACE cxx_std_20)
set_target_properties(dsp_core PROPERTIES CXX_EXTENSIONS OFF)

enable_testing()

add_executable(dsp_native_smoke test/native/native_smoke.cpp)
target_link_libraries(dsp_native_smoke PRIVATE dsp_core)
add_test(NAME dsp_native_smoke COMMAND dsp_native_smoke)
