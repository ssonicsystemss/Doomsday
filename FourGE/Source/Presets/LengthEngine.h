#pragma once
// =====================================================================
//  4GE module 14 - KICK LENGTH ENGINE tools (message thread: preset-like actions, no DSP / GUI)
//   SHORT ~100-250 ms / MEDIUM ~250-500 / LONG ~500-1000 / EXTRA LONG ~1000-2500 = the LENGTH knob set to the profile's value with
//   AUTO LENGTH on (the parts scale: PUNCH / BODY / TAIL / SUB each by its FOLLOW amount, envelopes and screech with them).
//   LENGTH SYNC (parameter) = a part of the beat at the host tempo. Independent of the STYLE (module 13): no style sets a length,
//   no length sets a style. One profile click = one UNDO.
// =====================================================================
#include "../PluginProcessor.h"

namespace fge::length
{
    enum Profile { Short = 0, Medium, Long, ExtraLong, NumProfiles };
    inline const juce::StringArray& names() { static const juce::StringArray a { "SHORT", "MEDIUM", "LONG", "EXTRA LONG" }; return a; }
    inline float profileMs (int p) { static const float v[NumProfiles] { 180.f, 400.f, 750.f, 1600.f }; return v[juce::jlimit (0, NumProfiles - 1, p)]; }
    inline int category (float ms) { return ms < 250.f ? Short : ms <= 500.f ? Medium : ms <= 1000.f ? Long : ExtraLong; }
    inline void applyProfile (GeProcessor& p, int prof)
    {
        p.undo.beginNewTransaction ("Length " + names()[prof]);
        p.setParamInUndoStep ("core_length", profileMs (prof));
        p.setParamInUndoStep ("core_auto_length", 1.f);
        p.setParamInUndoStep ("len_sync", 0.f);
    }
    // the label for the header: "MEDIUM 450 ms" / "SYNC 1 BEAT = 400 ms"
    inline juce::String label (const GeProcessor& p, double bpm)
    {
        const auto f = p.frame();
        const float ms = kickLengthMs (f, bpm);
        if (f.idx (P::lenSync) > 0) return "SYNC " + def (P::lenSync).choices()[f.idx (P::lenSync)] + " = " + juce::String (juce::roundToInt (ms)) + " ms";
        return names()[category (ms)] + "  " + juce::String (juce::roundToInt (ms)) + " ms";
    }
}
