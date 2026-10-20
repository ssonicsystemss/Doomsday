#pragma once
#include <juce_core/juce_core.h>

namespace ParamIDs
{
    // DELAY
    inline constexpr const char* dlyOn    = "dly_on";
    inline constexpr const char* dlyMode  = "dly_mode";
    inline constexpr const char* dlySync  = "dly_sync";
    inline constexpr const char* dlyDiv   = "dly_div";
    inline constexpr const char* dlyMs    = "dly_ms";
    inline constexpr const char* dlyFb    = "dly_fb";
    inline constexpr const char* dlyLo    = "dly_lo";
    inline constexpr const char* dlyHi    = "dly_hi";
    inline constexpr const char* dlyMod   = "dly_mod";
    inline constexpr const char* dlyDrive = "dly_drive";
    inline constexpr const char* dlyDuck  = "dly_duck";
    inline constexpr const char* dlyMix   = "dly_mix";
    // REVERB
    inline constexpr const char* revOn    = "rev_on";
    inline constexpr const char* revPre   = "rev_pre";
    inline constexpr const char* revSize  = "rev_size";
    inline constexpr const char* revDecay = "rev_decay";
    inline constexpr const char* revDamp  = "rev_damp";
    inline constexpr const char* revLo    = "rev_lo";
    inline constexpr const char* revMod   = "rev_mod";
    inline constexpr const char* revShim  = "rev_shim";
    inline constexpr const char* revDuck  = "rev_duck";
    inline constexpr const char* revWidth = "rev_width";
    inline constexpr const char* revMix   = "rev_mix";
    inline constexpr const char* freeze   = "rev_freeze";
    inline constexpr const char* d2r      = "d2r";        // delay into reverb
    // STEREO / OUTPUT
    inline constexpr const char* width    = "width";
    inline constexpr const char* spread   = "spread";
    inline constexpr const char* monoBass = "mono_bass";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* dry      = "dry";
    inline constexpr const char* output   = "output";
}

namespace Choices
{
    inline const juce::StringArray modes { "STEREO", "PING-PONG", "WIDE" };
    inline const juce::StringArray divisions { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                               "1/4T", "1/4", "1/4D", "1/2", "1/1" };
    inline const float divisionBeats[] { 0.125f, 1.f / 6.f, 0.25f, 0.375f, 1.f / 3.f, 0.5f, 0.75f,
                                         2.f / 3.f, 1.f, 1.5f, 2.f, 4.f };
}
