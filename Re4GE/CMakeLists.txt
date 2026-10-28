cmake_minimum_required(VERSION 3.22)
project(Re4GE VERSION 0.2.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)

# RE4GE - reverse bass instrument (split out of 4GE, shares its kick voice / stages / filters: Shared/Source/GeKick.h)
doomsday_plugin(Re4GE FLAT
    NAME "RE4GE"
    CODE Re4G
    VENDOR "sonicsystems"
    CATEGORIES Instrument Synth
    SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp
    TEST Tests/RenderTest.cpp)
