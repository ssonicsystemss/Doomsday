cmake_minimum_required(VERSION 3.22)
project(ScreamAlpha VERSION 0.4.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)

doomsday_plugin(ScreamAlpha
    NAME "5CR34M 4LPH4"
    CODE ScAl
    VENDOR "sonicsystems"
    CATEGORIES Instrument Synth
    FLAT
    SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp
    TEST Tests/RenderTest.cpp)
