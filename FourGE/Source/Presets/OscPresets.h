#pragma once
// =====================================================================
//  4GE module 03 - factory OSCILLATOR presets (one oscillator, applied to the selected one) in the spec's categories,
//  and the parameter groups of the oscillator tabs (what a tab file saves / loads). Data only.
// =====================================================================
#include <juce_core/juce_core.h>
#include <vector>

namespace fge
{
    struct OscPreset
    {
        const char* category;
        const char* name;
        std::vector<std::pair<const char*, float>> values;      // parameter suffix after "oscN_" -> plain value
    };
    inline const std::vector<OscPreset>& oscPresets()
    {
        static const std::vector<OscPreset> list {
            { "CLEAN SINE",        "Pure Sine",       { { "wave", 0 } } },
            { "CLEAN SINE",        "Sine Octave Up",  { { "wave", 0 }, { "octave", 1 } } },
            { "CLASSIC HARDSTYLE", "Hardstyle Table", { { "wave", 7 }, { "wt_table", 1 }, { "wt_position", 0.6f } } },
            { "CLASSIC HARDSTYLE", "Odd Harmonics",   { { "wave", 8 }, { "h_even", 0 }, { "h_rolloff", 0.15f } } },
            { "AGGRESSIVE SAW",    "Saw Stack",       { { "wave", 2 }, { "unison", 7 }, { "detune", 0.35f } } },
            { "AGGRESSIVE SAW",    "Wide Saw",        { { "wave", 2 }, { "unison", 9 }, { "detune", 0.5f }, { "spread", 1 } } },
            { "METALLIC",          "Metal Scan",      { { "wave", 7 }, { "wt_table", 3 }, { "wt_mode", 3 }, { "wt_scan", 160 } } },
            { "METALLIC",          "Metallic Noise",  { { "wave", 5 }, { "noise", 3 } } },
            { "FM TONES",          "FM Bell",         { { "wave", 0 }, { "ratio", 3.5f }, { "fm_feedback", 0.3f } } },
            { "FM TONES",          "FM Growl",        { { "wave", 2 }, { "fm_feedback", 0.6f }, { "fm_env", 0.8f }, { "mod_decay", 200 } } },
            { "SCREAM SOURCES",    "Ring Scream",     { { "wave", 2 }, { "ring_source", 5 }, { "ring_freq", 1300 }, { "ring_mix", 0.5f } } },
            { "SCREAM SOURCES",    "Formant Scream",  { { "wave", 7 }, { "wt_table", 2 }, { "wt_mode", 3 }, { "wt_scan", 180 } } },
            { "EXPERIMENTAL",      "Digital Noise",   { { "wave", 5 }, { "noise", 4 } } },
            { "EXPERIMENTAL",      "Random Scan",     { { "wave", 7 }, { "wt_mode", 4 }, { "wt_scan", 60 } } },
            { "SUB SOURCES",       "Sub Sine",        { { "wave", 0 }, { "track", 3 }, { "to_sub", 0 }, { "to_tail", -60 } } },
            { "SUB SOURCES",       "Sub Triangle",    { { "wave", 1 }, { "track", 3 }, { "to_sub", -3 }, { "to_tail", -60 } } },
        };
        return list;
    }
    inline juce::StringArray oscPresetCategories()
    {
        return { "CLEAN SINE", "CLASSIC HARDSTYLE", "AGGRESSIVE SAW", "METALLIC", "FM TONES", "SCREAM SOURCES", "EXPERIMENTAL", "SUB SOURCES" };
    }

    // the oscillator tabs and the parameters each one owns (suffix after "oscN_"; FM MATRIX also saves the fm_a_to_b matrix)
    enum OscTab { TabWave = 0, TabTable, TabHarm, TabUnison, TabFm, TabRing, TabPhase, TabSync, NumOscTabs };
    inline const char* oscTabName (int t)
    {
        static const char* n[NumOscTabs] { "WAVEFORM", "WAVETABLE", "HARMONICS", "UNISON", "FM MATRIX", "RING MOD", "PHASE", "HARD SYNC" };
        return n[juce::jlimit (0, NumOscTabs - 1, t)];
    }
    inline juce::StringArray oscTabParams (int t)
    {
        switch (t)
        {
            case TabWave:   return { "wave", "noise", "pw", "track", "freq", "ratio", "ratio_fine" };
            case TabTable:  return { "wt_table", "wt_position", "wt_morph", "wt_mode", "wt_start", "wt_end", "wt_scan", "seed" };
            case TabHarm:   return { "h_amount", "h_odd", "h_even", "h_rolloff", "h_tilt", "h_spread", "h_phase", "h_random", "seed" };
            case TabUnison: return { "unison", "detune", "spread", "blend", "unison_mode", "unison_phase", "seed" };
            case TabFm:     return { "ratio", "ratio_fine", "fm_feedback", "fm_env", "mod_decay" };
            case TabRing:   return { "ring_source", "ring_amount", "ring_mix", "ring_freq", "ring_env", "mod_decay" };
            case TabPhase:  return { "phase", "phase_mode", "phase_random", "phase_source", "invert" };
            case TabSync:   return { "sync_source", "sync_mode", "sync_amount" };
            default:        return {};
        }
    }
}
