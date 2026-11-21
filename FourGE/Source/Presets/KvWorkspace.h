#pragma once
// =====================================================================
//  4GE - KICK VARIATION workspace (owner 2026-11-20): the BASE kick and the versions MUTATE made from it.
//  Plain data held by the processor (message thread only, never the audio thread); the rules live in Presets/KickVariation.h.
// =====================================================================
#include "../Parameters/ParamFrame.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace fge::kv
{
    struct Workspace
    {
        bool hasBase = false;
        ParamFrame base;                        // BASE = the kick the versions come from
        std::vector<ParamFrame> versions;       // V1 .. V7
        std::vector<float> away;                // how far each version is from the base (0..100 %)
        juce::StringArray labels;               // what each version measured as ("brighter, heavier")
        std::vector<std::vector<float>> waveLo, waveHi;   // thumbnails: [0] = the BASE, then the versions
        int tried = 0, passed = 0; bool relaxed = false;  // candidates rendered / passing every check / tolerances relaxed
        int selected = -1;                      // -1 = BASE, 0.. = a version (what you hear now)
        int morphB = -1;                        // MORPH B = the last version picked
        float morph = 0.f;                      // MORPH A (base) -> B (selected version)
        bool fresh = false;                     // the versions = NEW KICK IN STYLE (from scratch), not MUTATE
        uint32_t generation = 0;                // +1 at every MUTATE (the GUI re-renders the thumbnails)
    };
}
