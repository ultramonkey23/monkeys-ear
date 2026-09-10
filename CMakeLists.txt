cmake_minimum_required(VERSION 3.20)
project(monkeys_ear VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags for hard real-time DSP performance
if(MSVC)
    add_compile_options(/O2 /fp:fast /W4)
else()
    add_compile_options(-O3 -ffast-math -Wall -Wextra -Wno-unused-parameter)
endif()

# Core DSP static library. Standalone module processors live here so the suite
# and individual VST3 wrappers share one DSP implementation.
add_library(monkeys_ear_core STATIC
    src/voice.cpp
    src/filter.cpp
    src/drive_tube.cpp
    src/resonator_cab.cpp
    src/delay.cpp
    src/fdn_reverb.cpp
    src/audio_input.cpp
    src/vocal_target.cpp
    src/vocal_module.cpp
    src/safety_limiter.cpp
    src/latency_meter.cpp
    src/preset.cpp
    src/chrono_state.cpp
    src/lfo.cpp
    src/tonal.cpp
    src/sound_space.cpp
    src/engine.cpp
)

target_include_directories(monkeys_ear_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Test Runner & Offline Renderer Executable
add_executable(monkeys_ear_test_runner
    test/test_dsp_core.cpp
)

target_include_directories(monkeys_ear_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

target_link_libraries(monkeys_ear_test_runner PRIVATE
    monkeys_ear_core
)

# Legacy combined VST3. Individual standalone wrappers are added beside this
# target as they satisfy the standalone module gate; do not duplicate DSP.
add_library(monkeys_ear SHARED
    vst3/vst3_entry.cpp
)

target_include_directories(monkeys_ear PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vst3
)

target_link_libraries(monkeys_ear PRIVATE
    monkeys_ear_core
)

set_target_properties(monkeys_ear PROPERTIES
    OUTPUT_NAME "monkeys_ear"
    SUFFIX ".vst3"
    PREFIX ""
)

# VST3 Host Verification Probe
if(WIN32)
    add_executable(monkeys_ear_host_probe
        test/test_vst3_host_probe.cpp
    )
    target_include_directories(monkeys_ear_host_probe PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vst3
    )
endif()
