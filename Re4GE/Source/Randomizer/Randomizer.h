#pragma once
// RE4GE - REVERSE BASS RANDOMIZER (signature feature 5). Message thread only (a button), never the audio thread.
//   MODE      FULL / OSC / PITCH / DIST / FILTER / MOD / REVERSE - which modules move
//   AMOUNT    how far from the current sound (0 = nothing, 1 = a new sound inside the musical ranges)
//   STYLE     CLASSIC / MODERN / RAW / EXPERIMENTAL - the ranges and the choices lean that way ("random in a direction")
//   LOCKS     modules that never move (OSC / PITCH / ENV / DIST / FILTER / MOD / SHAPE)
// Musical ranges only (not the knobs' full ranges), rules against bad combinations (the low end stays, the bass starts after the
// kick's tail, something always plays, SAMPLE only with a loaded sample), then the LOUDNESS is matched by an offline render of one
// bar in a private engine (2 passes). One click = one UNDO step.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/Engine.h"

class ReProcessor;

namespace re4ge
{
    class Randomizer
    {
    public:
        enum Mode { Full = 0, Osc, Pitch, Dist, Filter, Mod, Reverse, NumModes };
        enum Style { Classic = 0, Modern, Raw, Experimental, NumStyles };
        enum Group { GOsc = 0, GPitch, GEnv, GDist, GFilter, GMod, GShape, NumGroups };
        static const juce::StringArray& modeNames()  { static const juce::StringArray n { "FULL", "OSC", "PITCH", "DIST", "FILTER", "MOD", "REVERSE" }; return n; }
        static const juce::StringArray& styleNames() { static const juce::StringArray n { "CLASSIC", "MODERN", "RAW", "EXPERIMENTAL" }; return n; }
        static const juce::StringArray& groupNames() { static const juce::StringArray n { "OSC", "PITCH", "ENV", "DIST", "FILTER", "MOD", "SHAPE" }; return n; }

        // the settings live in the plugin state (saved with the project, not automatable)
        struct Settings { int mode = Full, style = Classic; float amount = 1.f; std::array<bool, NumGroups> lock {}; };
        static Settings read (const juce::ValueTree& state);
        static void write (juce::ValueTree& state, const Settings&);

        explicit Randomizer (ReProcessor& p) : proc (p) {}
        // returns the measured level (dB RMS) after matching
        float randomize (const Settings&, juce::Random&);
        // the loudness of the current sound: one bar at 150 BPM in a private engine (offline)
        float measureDb();
    private:
        float rollOnce (const Settings&, juce::Random&);
        void setParam (const char* id, float value);
        void setNorm (juce::RangedAudioParameter&, float value01);
        float getParam (const char* id) const;
        ReProcessor& proc;
        std::unique_ptr<re::Engine> offline;
        float lastPeak = 0.f;            // the peak of the last measurement (both beats)
    };
}
