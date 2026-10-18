#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Layers that play samples
enum Layer { Punch = 0, Crunch = 1, Kick = 2, NumSampleLayers = 3 };

namespace ParamIDs
{
    // per sample layer: prefix + suffix, e.g. "p_level", "c_pitch", "k_lp"
    inline const char* layerPrefix (int layer) { return layer == Punch ? "p_" : layer == Crunch ? "c_" : "k_"; }
    inline juce::String id (int layer, const char* suffix) { return juce::String (layerPrefix (layer)) + suffix; }

    // layer suffixes
    inline constexpr const char* on     = "on";
    inline constexpr const char* src    = "src";
    inline constexpr const char* level  = "level";
    inline constexpr const char* pitch  = "pitch";
    inline constexpr const char* offset = "offset";   // v2: time offset in ms (negative = trim the start)
    inline constexpr const char* hp     = "hp";
    inline constexpr const char* lp     = "lp";
    inline constexpr const char* rev    = "rev";      // v2: play the sample reversed
    inline constexpr const char* forge  = "forge";    // v2: send this layer through FORGE
    inline constexpr const char* punchKey = "p_key";      // punch key-tracking is optional

    // FORGE (v2)
    inline constexpr const char* fOn     = "f_on";
    inline constexpr const char* fDrive  = "f_drive";
    inline constexpr const char* fFocus  = "f_focus";
    inline constexpr const char* fGrit   = "f_grit";
    inline constexpr const char* fCrunch = "f_crunch";
    inline constexpr const char* fTone   = "f_tone";
    inline constexpr const char* fOtt    = "f_ott";
    inline constexpr const char* fSmooth = "f_smooth";   // v2.1
    inline constexpr const char* fMix    = "f_mix";      // v2.1

    // MASTER
    inline constexpr const char* root     = "root";
    inline constexpr const char* preGain  = "pregain";
    inline constexpr const char* clipOn   = "clip_on";
    inline constexpr const char* clipCeil = "clip_ceil";
    inline constexpr const char* clipKnee = "clip_knee";
    inline constexpr const char* limOn    = "lim_on";
    inline constexpr const char* limCeil  = "lim_ceil";
    inline constexpr const char* limRel   = "lim_rel";
    inline constexpr const char* volume   = "volume";
}

namespace Library
{
    inline constexpr int perLayer = 8;
    inline const juce::StringArray punchNames  { "Bronze Tap", "Temple Click", "Obsidian Hit", "Sun Stamp",
                                                 "Iron Knock", "Marble Snap", "Ritual Thump", "Granite Pop" };
    inline const juce::StringArray crunchNames { "Sand Grind", "Ash Crush", "Desert Storm", "Bone Rattle",
                                                 "Ember Tear", "Rust Shred", "Tomb Static", "Scorch Bite" };
    inline const juce::StringArray kickNames   { "Pharaoh", "Colossus", "Babylon", "Sphinx",
                                                 "Olympus", "Monolith", "Anubis", "Titan" };
    inline const juce::String userItem = "USER SAMPLE";
    inline constexpr int userIndex = perLayer;               // choice index meaning "user sample"

    inline juce::StringArray choicesFor (int layer)
    {
        auto a = layer == Punch ? punchNames : layer == Crunch ? crunchNames : kickNames;
        a.add (userItem);
        return a;
    }

    // all library tails are tuned to this pitch (C2); playing the ROOT note = no transposition
    inline constexpr float tailHz = 65.406f;
}
