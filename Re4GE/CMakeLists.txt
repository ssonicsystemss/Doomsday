cmake_minimum_required(VERSION 3.22)
project(Re4GE VERSION 0.15.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/cmake/Doomsday.cmake)

# RE4GE - reverse bass instrument (split out of 4GE, shares its kick voice / stages / filters: Shared/Source/GeKick.h)
doomsday_plugin(Re4GE FLAT
    NAME "RE4GE"
    CODE Re4G
    VENDOR "sonicsystems"
    CATEGORIES Instrument Synth
    SOURCES Source/PluginProcessor.cpp Source/GUI/PluginEditor.cpp Source/GUI/ShapeDesigner.cpp Source/GUI/WaveshaperView.cpp Source/GUI/MatrixView.cpp Source/GUI/KickView.cpp Source/GUI/RandomPanel.cpp Source/GUI/SeqView.cpp Source/GUI/EqView.cpp Source/GUI/ExportPanel.cpp Source/GUI/MorphBar.cpp Source/Parameters/Layout.cpp Source/Parameters/ParamBridge.cpp
            Source/Presets/FactoryPresets.cpp Source/Presets/SampleFile.cpp Source/Randomizer/Randomizer.cpp
    TEST Tests/RenderTest.cpp)

# RE4GE FX: the same plugin as an effect - the kicks come in as audio (input / sidechain), LIVE reverse of the previous kick
doomsday_plugin(Re4GEFX EFFECT FLAT
    NAME "RE4GE FX"
    CODE Re4F
    VENDOR "sonicsystems"
    CATEGORIES Fx Distortion
    SOURCES Source/PluginProcessor.cpp Source/GUI/PluginEditor.cpp Source/GUI/ShapeDesigner.cpp Source/GUI/WaveshaperView.cpp Source/GUI/MatrixView.cpp Source/GUI/KickView.cpp Source/GUI/RandomPanel.cpp Source/GUI/SeqView.cpp Source/GUI/EqView.cpp Source/GUI/ExportPanel.cpp Source/GUI/MorphBar.cpp Source/Parameters/Layout.cpp Source/Parameters/ParamBridge.cpp
            Source/Presets/FactoryPresets.cpp Source/Presets/SampleFile.cpp Source/Randomizer/Randomizer.cpp)
target_compile_definitions(Re4GEFX PRIVATE RE4GE_FX=1)

