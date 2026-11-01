#pragma once
#include "DoomsdayParams.h"

namespace re4ge
{
    struct FactoryPreset { juce::String name; doomsday::PresetValues v; };
    const std::vector<FactoryPreset>& factoryPresets();
    // envelope templates PUMP / SWELL / TIGHT / ROLL / PULSE: only the envelope / step knobs
    const juce::StringArray& templateNames();
    const std::vector<doomsday::PresetValues>& templates();
}
