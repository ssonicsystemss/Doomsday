cmake_minimum_required(VERSION 3.22)
project(DarkSerpent VERSION 1.0.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)

doomsday_plugin(DarkSerpent
    NAME "Dark Serpent"
    CODE DkSp
    CATEGORIES Instrument Synth
    SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp
    TEST Tests/RenderTest.cpp)
