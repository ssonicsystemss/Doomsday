#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamIDs.h"
#include "../DSP/ReParams.h"

namespace re4ge
{
    struct ParamCache { std::vector<std::atomic<float>*> ptrs; };
    re::Params gatherParams (const juce::AudioProcessorValueTreeState&, ParamCache&);
}
