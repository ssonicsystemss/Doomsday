cmake_minimum_required(VERSION 3.22)
project(FourTune VERSION 0.1.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)

# 4TUNE - hardstyle melody synthesizer, successor of Fortune of Seekers (frozen at v1.0); spec SPEC.md, plan PLAN.md
doomsday_plugin(FourTune
    NAME "4TUNE"
    CODE FrTn
    VENDOR "sonicsystems"
    CATEGORIES Instrument Synth
    FLAT
    SOURCES Source/Params.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/PianoRoll.cpp Source/MelodyPage.cpp Source/PresetPage.cpp
    TEST Tests/RenderTest.cpp)
