cmake_minimum_required(VERSION 3.22)
project(MirrorOfSouls VERSION 1.0.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)

doomsday_plugin(MirrorOfSouls
    EFFECT
    NAME "Mirror of Souls"
    CODE MoSl
    CATEGORIES Fx EQ
    SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp
    TEST Tests/RenderTest.cpp)
