cmake_minimum_required(VERSION 3.22)
project(AncientDrums VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# static MSVC runtime -> the .vst3 runs on any Windows PC without extra redistributables
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: use ./JUCE if present, otherwise download it automatically
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(AncientData SOURCES
    Resources/Fonts/Cinzel-VF.ttf
    Resources/Fonts/Michroma-Regular.ttf
    Resources/Fonts/BarlowSemiCondensed-Medium.ttf
    Resources/Fonts/BarlowSemiCondensed-SemiBold.ttf
    Resources/Fonts/JetBrainsMono-VF.ttf)
set_target_properties(AncientData PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(AncientDrums
    COMPANY_NAME "Prophecy Audio"
    PLUGIN_MANUFACTURER_CODE Prph
    PLUGIN_CODE AnDr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Ancient Drums"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum)

target_sources(AncientDrums PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(AncientDrums PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    # optional X11 extensions (Linux only, irrelevant on Windows)
    $<$<PLATFORM_ID:Linux>:JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0>)

target_link_libraries(AncientDrums
    PRIVATE
        AncientData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- offline test (optional)
option(PROPHECY_BUILD_TESTS "Build offline render test" OFF)
if(PROPHECY_BUILD_TESTS)
    juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")
    target_sources(RenderTest PRIVATE Tests/RenderTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(RenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
        $<$<PLATFORM_ID:Linux>:JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0>)
    target_link_libraries(RenderTest PRIVATE AncientData juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()
