#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================
//  FORTUNE OF SEEKERS - parameter IDs
//  part prefix  "a_" / "b_"           (PART A = green, PART B = purple)
//  osc prefix   "a1_" "a2_" "b1_" "b2_"
// =====================================================================
namespace ParamIDs
{
    inline constexpr int numParts = 2;
    inline constexpr int oscsPerPart = 2;

    inline juce::String part (int p, const char* base) { return juce::String (p == 0 ? "a_" : "b_") + base; }
    inline juce::String osc (int p, int o, const char* base)
    {
        return juce::String (p == 0 ? "a" : "b") + juce::String (o + 1) + "_" + base;
    }

    // oscillator
    inline constexpr const char* on     = "on";
    inline constexpr const char* wave   = "wave";
    inline constexpr const char* voices = "voices";
    inline constexpr const char* oct    = "oct";
    inline constexpr const char* semi   = "semi";
    inline constexpr const char* vol    = "vol";
    inline constexpr const char* phase  = "phase";
    inline constexpr const char* detune = "detune";
    inline constexpr const char* stereo = "stereo";
    inline constexpr const char* pan    = "pan";
    inline constexpr const char* scream = "scream";

    // part: filter + envelopes + level
    inline constexpr const char* partOn  = "on";
    inline constexpr const char* ftype   = "ftype";
    inline constexpr const char* cutoff  = "cutoff";
    inline constexpr const char* reso    = "reso";
    inline constexpr const char* fdrive  = "fdrive";
    inline constexpr const char* fenv    = "fenv";
    inline constexpr const char* fdecay  = "fdecay";
    inline constexpr const char* attack  = "att";
    inline constexpr const char* decay   = "dec";
    inline constexpr const char* sustain = "sus";
    inline constexpr const char* release = "rel";
    inline constexpr const char* level   = "level";

    // part FX
    inline constexpr const char* distOn    = "dist_on";
    inline constexpr const char* distType  = "dist_type";
    inline constexpr const char* distDrive = "dist_drive";
    inline constexpr const char* distMix   = "dist_mix";
    inline constexpr const char* choOn     = "cho_on";
    inline constexpr const char* choRate   = "cho_rate";
    inline constexpr const char* choDepth  = "cho_depth";
    inline constexpr const char* choMix    = "cho_mix";
    inline constexpr const char* dlyOn     = "dly_on";
    inline constexpr const char* dlyDiv    = "dly_div";
    inline constexpr const char* dlyFb     = "dly_fb";
    inline constexpr const char* dlyMix    = "dly_mix";
    inline constexpr const char* dlyPP     = "dly_pp";
    inline constexpr const char* revOn     = "rev_on";
    inline constexpr const char* revSize   = "rev_size";
    inline constexpr const char* revDamp   = "rev_damp";
    inline constexpr const char* revMix    = "rev_mix";
    inline constexpr const char* pumpOn    = "pump_on";
    inline constexpr const char* pumpDiv   = "pump_div";
    inline constexpr const char* pumpDepth = "pump_depth";
    inline constexpr const char* pumpShape = "pump_shape";

    // global
    inline constexpr const char* mono    = "mono";
    inline constexpr const char* glide   = "glide";
    inline constexpr const char* fortune = "fortune";
    inline constexpr const char* volume  = "volume";
}

namespace Choices
{
    inline const juce::StringArray waves      { "SINE", "TRI", "SAW", "SQUARE", "PULSE", "NOISE" };
    inline const juce::StringArray waveShort  { "SIN", "TRI", "SAW", "SQR", "PLS", "NSE" };
    inline const juce::StringArray filters    { "LP12", "LP24", "BAND", "HIGH" };
    inline const juce::StringArray dists      { "SOFT", "HARD", "TUBE", "SCREAM" };
    inline const juce::StringArray delayDivs  { "1/16", "1/8 T", "1/8", "1/8 D", "1/4", "1/4 D", "1/2" };
    inline const float delayBeats[]           { 0.25f, 1.f / 3.f, 0.5f, 0.75f, 1.f, 1.5f, 2.f };
    inline const juce::StringArray pumpDivs   { "1/8", "1/4", "1/2", "1/1" };
    inline const float pumpBeats[]            { 0.5f, 1.f, 2.f, 4.f };
}
