cmake_minimum_required(VERSION 3.22)
project(AbyssOfEchoes VERSION 1.0.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)

doomsday_plugin(AbyssOfEchoes
    EFFECT
    NAME "Abyss of Echoes"
    CODE AbEc
    CATEGORIES Fx Reverb Delay
    SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp
    TEST Tests/RenderTest.cpp)
