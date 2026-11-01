#pragma once
#include "DoomsdayParams.h"
#include "DoomsdayMod.h"
#include <array>

namespace re4ge
{
    struct FactoryPreset { juce::String name; doomsday::PresetValues v; std::array<juce::String, 4> shapes {}; };   // shapes: AMP / PITCH / HARM / PHASE (empty = default)
    const std::vector<FactoryPreset>& factoryPresets();
    // envelope templates PUMP / SWELL / TIGHT / ROLL / PULSE: only the envelope / step knobs
    const juce::StringArray& templateNames();
    const std::vector<doomsday::PresetValues>& templates();
    // the SHAPE DESIGNER lanes' default curves (time 0..1000 = the lane length)
    doomsday::mod::Shape defaultLaneShape (int lane);
}
