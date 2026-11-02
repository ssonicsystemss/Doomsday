#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamIDs.h"
#include "../DSP/ReParams.h"

namespace re4ge
{
    struct ParamCache { std::vector<std::atomic<float>*> ptrs; std::vector<juce::RangedAudioParameter*> prm; };
    // MORPH A / B: two snapshots (normalized, in the cache's order); skip = parameters that never morph (the morph controls)
    struct MorphView { const float* a = nullptr; const float* b = nullptr; const char* skip = nullptr; float t = 0.f; };
    re::Params gatherParams (const juce::AudioProcessorValueTreeState&, ParamCache&, const MorphView* morph = nullptr);
}
