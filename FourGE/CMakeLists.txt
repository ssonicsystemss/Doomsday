cmake_minimum_required(VERSION 3.22)
project(FourGE VERSION 0.3.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)

# 4GE - hardstyle kick synth, successor of Forge of Wrath (frozen at v2.1)
doomsday_plugin(FourGE FLAT
    NAME "4GE"
    CODE FoGE
    VENDOR "sonicsystems"
    CATEGORIES Instrument Drum
    SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp
    TEST Tests/RenderTest.cpp)
