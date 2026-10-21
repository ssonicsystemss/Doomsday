cmake_minimum_required(VERSION 3.22)
project(ProphecyOfScream VERSION 3.1.0)

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

juce_add_binary_data(ProphecyData SOURCES
    Shared/Resources/Fonts/Inter-Medium.ttf
    Shared/Resources/Fonts/Inter-SemiBold.ttf
    Shared/Resources/Fonts/Inter-Bold.ttf
    Resources/Presets/Blippy_Blippy.pos
    Resources/Presets/Brain_Fryer.pos
    Resources/Presets/Cardio.pos
    Resources/Presets/Devils_Scream.pos
    Resources/Presets/Farting_Grandpa.pos
    Resources/Presets/Horn_of_Fallen.pos
    Resources/Presets/Mad_Machine.pos
    Resources/Presets/Sexy_Nightmare.pos
    Resources/Presets/Sonar.pos
    Resources/Presets/Zephyr.pos)
set_target_properties(ProphecyData PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(ProphecyOfScream
    COMPANY_NAME "Prophecy Audio"
    PLUGIN_MANUFACTURER_CODE Prph
    PLUGIN_CODE PoSc
    FORMATS VST3 Standalone
    PRODUCT_NAME "Prophecy of Scream"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

target_sources(ProphecyOfScream PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Shared/Source/DoomsdayFlat.cpp)

target_compile_definitions(ProphecyOfScream PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    # optional X11 extensions (Linux only, irrelevant on Windows)
    $<$<PLATFORM_ID:Linux>:JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0>)

target_link_libraries(ProphecyOfScream
    PRIVATE
        ProphecyData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- offline test (optional)
option(PROPHECY_BUILD_TESTS "Build offline render test" OFF)
if(PROPHECY_BUILD_TESTS)
    juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")
    target_sources(RenderTest PRIVATE Tests/RenderTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Shared/Source/DoomsdayFlat.cpp)
    target_compile_definitions(RenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        $<$<PLATFORM_ID:Linux>:JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0>)
    target_link_libraries(RenderTest PRIVATE ProphecyData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

# ---------------------------------------------------------------- TikTok promo renderer (optional, Linux)
# Renders the real plugin (audio + GUI) into raw frames; see Promo/README.md
option(PROPHECY_BUILD_PROMO "Build the promo video renderer" OFF)
if(PROPHECY_BUILD_PROMO)
    juce_add_console_app(PromoRender PRODUCT_NAME "PromoRender")
    target_sources(PromoRender PRIVATE Promo/promo.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Shared/Source/DoomsdayFlat.cpp)
    target_include_directories(PromoRender PRIVATE Source Promo)
    target_compile_definitions(PromoRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        $<$<PLATFORM_ID:Linux>:JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0>)
    target_link_libraries(PromoRender PRIVATE ProphecyData juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats
        juce::juce_recommended_config_flags)
endif()
