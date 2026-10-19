#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================
//  DARK SERPENT - parameter IDs and choices
// =====================================================================
namespace ParamIDs
{
    // CORE (the main supersaw + its octave copy)
    inline constexpr const char* coreWave = "core_wave", *coreVoices = "core_voices", *coreDetune = "core_detune",
                                 *coreOct = "core_oct", *coreLevel = "core_level";
    // EDGE (bright, wide top layer)
    inline constexpr const char* edgeOn = "edge_on", *edgeVoices = "edge_voices", *edgeDetune = "edge_detune",
                                 *edgeOct = "edge_oct", *edgeTone = "edge_tone", *edgeLevel = "edge_level";
    // BODY (clean mono centre)
    inline constexpr const char* bodyOn = "body_on", *bodyWave = "body_wave", *bodyOct = "body_oct", *bodyLevel = "body_level";
    // FILTER + TALK (formant)
    inline constexpr const char* cutoff = "cutoff", *tone = "tone", *reso = "reso", *fenv = "fenv", *fdecay = "fdecay";
    inline constexpr const char* talk = "talk", *vowel = "vowel", *sweep = "sweep", *sweepTime = "sweep_time", *talkKey = "talk_key";
    // MOVE (tempo-synced LFO)
    inline constexpr const char* moveOn = "move_on", *moveDiv = "move_div", *moveShape = "move_shape", *moveCut = "move_cut",
                                 *moveVowel = "move_vowel", *moveAmp = "move_amp", *moveRetrig = "move_retrig";
    // PLAY
    inline constexpr const char* mode = "mode", *glide = "glide", *bendAmt = "bend_amt", *bendTime = "bend_time", *bendFirst = "bend_first",
                                 *vibDepth = "vib_depth", *vibRate = "vib_rate", *vibDelay = "vib_delay", *velocity = "vel";
    // RAGE
    inline constexpr const char* rageOn = "rage_on", *rageType = "rage_type", *rageDrive = "rage_drive", *rageMix = "rage_mix", *ott = "ott";
    // AMP
    inline constexpr const char* attack = "att", *decay = "dec", *sustain = "sus", *release = "rel";
    // SPACE
    inline constexpr const char* width = "width", *monoBass = "mono_bass";
    // FX
    inline constexpr const char* dlyOn = "dly_on", *dlyDiv = "dly_div", *dlyFb = "dly_fb", *dlyMix = "dly_mix";
    inline constexpr const char* revOn = "rev_on", *revSize = "rev_size", *revMix = "rev_mix";
    inline constexpr const char* pumpOn = "pump_on", *pumpDiv = "pump_div", *pumpDepth = "pump_depth";
    // global
    inline constexpr const char* energy = "energy", *volume = "volume";
}

namespace Choices
{
    inline const juce::StringArray coreWaves { "SAW", "SQUARE", "PULSE", "HYBRID" };
    inline const int coreWaveIds[]           { 2, 3, 4, 5 };
    inline const juce::StringArray bodyWaves { "SINE", "TRI", "SQUARE" };
    inline const int bodyWaveIds[]           { 0, 1, 3 };
    inline const juce::StringArray edgeOcts  { "+1 OCT", "+2 OCT" };
    inline const juce::StringArray moveDivs  { "1/1", "1/2", "1/4", "1/4 T", "1/8", "1/8 T", "1/16" };
    inline const float moveBeats[]           { 4.f, 2.f, 1.f, 2.f / 3.f, 0.5f, 1.f / 3.f, 0.25f };
    inline const juce::StringArray shapes    { "SINE", "TRI", "SAW", "SQR", "S&H" };
    inline const juce::StringArray modes     { "LEGATO", "RETRIG", "POLY" };
    inline const juce::StringArray rages     { "TUBE", "HARD", "FOLD" };
    inline const juce::StringArray delayDivs { "1/16", "1/8 T", "1/8", "1/8 D", "1/4", "1/4 D", "1/2" };
    inline const float delayBeats[]          { 0.25f, 1.f / 3.f, 0.5f, 0.75f, 1.f, 1.5f, 2.f };
    inline const juce::StringArray pumpDivs  { "1/8", "1/4", "1/2", "1/1" };
    inline const float pumpBeats[]           { 0.5f, 1.f, 2.f, 4.f };
    inline const char* vowels = "AEIOU";
}
