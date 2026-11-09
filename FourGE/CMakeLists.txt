cmake_minimum_required(VERSION 3.22)
project(FourGE VERSION 0.5.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)

# 4GE - hardstyle kick synth, successor of Forge of Wrath (frozen at v2.1).
# v0.5 = the rebuild from scratch (17 modules, FourGE/PLAN.md). NEW plugin code (was FoGE): the parameters are all new, so old FL projects
# with 4GE v0.4 keep working with the old 4GE.vst3 (renamed, side by side) instead of silently loading the new engine with defaults.
doomsday_plugin(FourGE FLAT
    NAME "4GE"
    CODE Fg5K
    VENDOR "sonicsystems"
    CATEGORIES Instrument Drum
    SOURCES Source/PluginProcessor.cpp Source/GUI/PluginEditor.cpp Source/GUI/WaveView.cpp Source/GUI/EnvEditor.cpp Source/GUI/EnvPage.cpp Source/GUI/OscPage.cpp Source/GUI/OscScope.cpp Source/GUI/LayerPage.cpp Source/GUI/DistPage.cpp Source/GUI/EqPage.cpp Source/GUI/DynPage.cpp Source/GUI/ModPage.cpp Source/GUI/SamplePage.cpp Source/GUI/StereoPage.cpp Source/GUI/ExportPage.cpp Source/GUI/ScreechPage.cpp Source/Parameters/ParamRegistry.cpp Source/Presets/FactoryPresets.cpp
    TEST Tests/RenderTest.cpp)
# module 10: MP3 import (optional format of the sample engine)
foreach(t FourGE RenderTest)
    if(TARGET ${t})
        target_compile_definitions(${t} PRIVATE JUCE_USE_MP3AUDIOFORMAT=1)
    endif()
endforeach()
