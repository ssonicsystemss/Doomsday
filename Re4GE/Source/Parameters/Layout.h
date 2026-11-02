#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamIDs.h"

namespace re4ge
{
    // fx = the RE4GE FX build (adds the fx_* parameters)
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout (bool fx = false);
}
