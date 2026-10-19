# =====================================================================
#  DOOMSDAY shared CMake helper
#
#  In a plugin folder (e.g. DarkSerpent/CMakeLists.txt):
#    cmake_minimum_required(VERSION 3.22)
#    project(DarkSerpent VERSION 1.0.0)
#    include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)
#    doomsday_plugin(DarkSerpent
#        NAME "Dark Serpent"  CODE DkSp  CATEGORIES Instrument Synth
#        EFFECT                                        (optional: audio effect instead of an instrument)
#        SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp
#        RESOURCES <extra binary data files>          (optional)
#        TEST Tests/RenderTest.cpp)                    (optional, built with -DPROPHECY_BUILD_TESTS=ON)
#
#  Gives the plugin: JUCE 8.0.10, static MSVC runtime, VST3 + Standalone, the shared fonts
#  in BinaryData, Shared/Source on the include path and DoomsdayUI.cpp compiled in.
# =====================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(DOOMSDAY_SHARED "${CMAKE_CURRENT_LIST_DIR}/..")

if(NOT TARGET juce::juce_core)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/JUCE" JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

function(doomsday_plugin TARGET)
    cmake_parse_arguments(D "EFFECT" "NAME;CODE;TEST" "SOURCES;RESOURCES;CATEGORIES" ${ARGN})
    set(data ${TARGET}Data)
    if(D_EFFECT)
        set(isSynth FALSE)
    else()
        set(isSynth TRUE)
    endif()
    juce_add_binary_data(${data} SOURCES
        ${DOOMSDAY_SHARED}/Resources/Fonts/Michroma-Regular.ttf
        ${DOOMSDAY_SHARED}/Resources/Fonts/BarlowSemiCondensed-Medium.ttf
        ${DOOMSDAY_SHARED}/Resources/Fonts/BarlowSemiCondensed-SemiBold.ttf
        ${DOOMSDAY_SHARED}/Resources/Fonts/BarlowSemiCondensed-Bold.ttf
        ${D_RESOURCES})
    set_target_properties(${data} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    juce_add_plugin(${TARGET}
        COMPANY_NAME "Prophecy Audio"
        PLUGIN_MANUFACTURER_CODE Prph
        PLUGIN_CODE ${D_CODE}
        FORMATS VST3 Standalone
        PRODUCT_NAME "${D_NAME}"
        IS_SYNTH ${isSynth}
        NEEDS_MIDI_INPUT ${isSynth}
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES ${D_CATEGORIES})

    set(defs JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        $<$<PLATFORM_ID:Linux>:JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0>)
    target_sources(${TARGET} PRIVATE ${D_SOURCES} ${DOOMSDAY_SHARED}/Source/DoomsdayUI.cpp)
    target_include_directories(${TARGET} PRIVATE ${DOOMSDAY_SHARED}/Source)
    target_compile_definitions(${TARGET} PUBLIC ${defs})
    target_link_libraries(${TARGET}
        PRIVATE ${data} juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

    option(PROPHECY_BUILD_TESTS "Build offline render test" OFF)
    if(PROPHECY_BUILD_TESTS AND D_TEST)
        juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")
        target_sources(RenderTest PRIVATE ${D_TEST} ${D_SOURCES} ${DOOMSDAY_SHARED}/Source/DoomsdayUI.cpp)
        target_include_directories(RenderTest PRIVATE ${DOOMSDAY_SHARED}/Source)
        target_compile_definitions(RenderTest PRIVATE ${defs})
        target_link_libraries(RenderTest PRIVATE ${data} juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    endif()
endfunction()
