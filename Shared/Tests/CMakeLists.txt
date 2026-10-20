# Tests for the Shared blocks (no plugin needed):  cmake -S . -B build -G Ninja && ninja -C build && build/SharedTests_artefacts/Release/SharedTests
cmake_minimum_required(VERSION 3.22)
project(SharedTests VERSION 1.0.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Doomsday.cmake)
juce_add_console_app(SharedTests PRODUCT_NAME "SharedTests")
target_sources(SharedTests PRIVATE SharedTests.cpp)
target_include_directories(SharedTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Source)
target_compile_definitions(SharedTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SharedTests PRIVATE juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
