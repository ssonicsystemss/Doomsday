#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================
//  MIRROR OF SOULS - parameter IDs (8 bands: "b1_" .. "b8_")
// =====================================================================
namespace ParamIDs
{
    inline constexpr int numBands = 8;
    inline juce::String band (int b, const char* key) { return "b" + juce::String (b + 1) + "_" + key; }
    inline constexpr const char* on = "on", *type = "type", *slope = "slope", *freq = "freq", *gain = "gain", *q = "q",
                                 *mode = "mode", *dyn = "dyn", *thresh = "thr";
    inline constexpr const char* amount = "amount", *output = "output", *monoBass = "mono_bass", *analyzer = "analyzer";
}

// band gain: cuts go deeper than boosts (-48 .. +24 dB), 0 dB stays in the middle of the knob
inline juce::NormalisableRange<float> gainRange()
{
    return { -48.f, 24.f,
             [] (float, float, float v) { return v < 0.5f ? -48.f + 96.f * v : (v - 0.5f) * 48.f; },
             [] (float, float, float db) { return db < 0.f ? (db + 48.f) / 96.f : 0.5f + db / 48.f; },
             [] (float, float, float db) { return juce::jlimit (-48.f, 24.f, db); } };
}

namespace Choices
{
    enum Type { LowCut, LowShelf, Bell, Notch, HighShelf, HighCut };
    inline const juce::StringArray types      { "LOW CUT", "LOW SHELF", "BELL", "NOTCH", "HIGH SHELF", "HIGH CUT" };
    inline const juce::StringArray typesShort { "L.CUT", "L.SHELF", "BELL", "NOTCH", "H.SHELF", "H.CUT" };
    inline const juce::StringArray slopes     { "12 dB", "24 dB", "48 dB", "96 dB" };
    inline const juce::StringArray modes      { "STEREO", "MID", "SIDE" };
    inline const juce::StringArray analyzers  { "OFF", "POST", "PRE + POST" };
    // default layout of the 8 bands (all flat)
    inline const float defaultFreq[8] { 30.f, 90.f, 250.f, 700.f, 2000.f, 5000.f, 10000.f, 18000.f };
    inline const int   defaultType[8] { LowCut, LowShelf, Bell, Bell, Bell, Bell, HighShelf, HighCut };
}
