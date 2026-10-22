cmake_minimum_required(VERSION 3.22)
project(ForgeOfWrath VERSION 2.1.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)

doomsday_plugin(ForgeOfWrath
    NAME "Forge of Wrath"
    CODE FoWr
    CATEGORIES Instrument Drum
    SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp
    TEST Tests/RenderTest.cpp)
