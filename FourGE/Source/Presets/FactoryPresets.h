#pragma once
// 4GE factory presets: { name, values that differ from the defaults (by parameter ID) }. Data only - no DSP, no GUI.
#include "DoomsdayParams.h"
#include <vector>

namespace fge
{
    struct FactoryPreset { juce::String name; doomsday::PresetValues values; };
    const std::vector<FactoryPreset>& factoryPresets();
}
