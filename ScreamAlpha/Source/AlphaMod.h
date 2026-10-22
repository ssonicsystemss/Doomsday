#pragma once
// =====================================================================
//  5CR34M 4LPH4 - MOD 1 / MOD 2 (the owner's design, 2026-10-22)
//
//  Each MOD = one cycle of a shape (Rise / Fall / Up-Down / Wobble / Steps / CUSTOM drawn with points), 1/16 .. 1 bar,
//  locked to the host tempo, assigned to ONE knob with the ASSIGN button.
//  Range = one slider + a [+ / -] switch; the slider is the OTHER END of the movement as an absolute knob position:
//      +  : knob position -> up to the slider     (slider can't go below the knob)
//      -  : knob position -> down to the slider   (slider can't go above the knob)
//  value01 = base01 + shape * (end01 - base01)       (normalised knob travel, so it follows each knob's own scale)
//
//  MOD 1 keeps the old cutoff-automation parameters (autoShape / autoDiv); old projects with "Depth" (autoOct) are
//  converted to MOD 1 -> CUTOFF when loaded (AlphaProcessor::migrateLegacyAuto).
// =====================================================================
#include <juce_core/juce_core.h>
#include <cstring>
#include "../../Shared/Source/DoomsdayMod.h"

namespace alpha::mod
{
    constexpr int numMods = 2;

    // knobs a MOD can be assigned to. The index is saved in projects (m1Target / m2Target): ONLY APPEND, never reorder.
    inline const char* const targets[] {
        "scoopSt", "scoopMs", "scoopCurve", "glideMs", "tuneSt", "coreLevel", "coreShape",
        "hashLevel", "hashDecayMs", "hashSustain", "detuneCents", "carrierSaw", "ratio", "index", "chaos", "chaosHz", "grit", "hashHp",
        "drive1Db", "combMix", "combFb", "combRatio", "formHz", "formGainDb", "formQ",
        "drive2Db", "toneHz", "lowCut", "attackMs", "decayMs", "sustain", "releaseMs", "chorus", "outDb",
        "cutoffHz", "reso", "fortune", "ott", "dlyMix", "dlyFb", "revMix", "revSize", "revDamp"
    };
    constexpr int numTargets = (int) (sizeof (targets) / sizeof (targets[0]));
    // 1-based choice index (0 = none)
    inline int targetIndex (const char* id)
    {
        for (int i = 0; i < numTargets; ++i) if (std::strcmp (targets[i], id) == 0) return i + 1;
        return 0;
    }
    inline const char* targetId (int index) { return index >= 1 && index <= numTargets ? targets[index - 1] : nullptr; }

    enum ShapeType { Off = 0, Rise, Fall, UpDown, Wobble, Steps, Custom };

    // the parameter IDs of MOD m (MOD 1 = the old cutoff automation IDs, never renamed)
    inline const char* shapeId (int m)  { return m == 0 ? "autoShape" : "m2Shape"; }
    inline const char* divId (int m)    { return m == 0 ? "autoDiv" : "m2Div"; }
    inline const char* targetPid (int m){ return m == 0 ? "m1Target" : "m2Target"; }
    inline const char* rangeId (int m)  { return m == 0 ? "m1Range" : "m2Range"; }
    inline const char* polId (int m)    { return m == 0 ? "m1Pol" : "m2Pol"; }
    inline const char* customKey (int m){ return m == 0 ? "m1Custom" : "m2Custom"; }

    // STEPS: a new random level every 16th (seeded per MOD)
    inline float stepLevel (long long step, int seed)
    {
        uint32_t h = (uint32_t) (step + 7919 * seed) * 2654435761u; h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        return (float) (h & 0xffff) / 65535.f;
    }

    // ready shapes, 0..1 at a beat position (len = cycle length in beats)
    inline float builtin (int type, double beat, double len, int seed)
    {
        const double ph = beat / len - std::floor (beat / len);
        const float f = (float) ph;
        switch (type)
        {
            case Rise:   return f;
            case Fall:   return 1.f - f;
            case UpDown: return 1.f - std::abs (2.f * f - 1.f);
            case Wobble: return 0.5f + 0.5f * std::sin (f * juce::MathConstants<float>::twoPi - 1.5707963f);
            case Steps:  return stepLevel ((long long) std::floor (beat * 4.0 + 1e-9), seed);
            default:     return 0.f;
        }
    }

    // a ready shape as points (x = 0..1000 of one cycle), so it can be edited in CUSTOM
    inline doomsday::mod::Shape toPoints (int type, double beats, int seed)
    {
        using S = doomsday::mod::Shape;
        switch (type)
        {
            case Fall:   return S::make ({ { 0.f, 1.f }, { 1000.f, 0.f } });
            case UpDown: return S::make ({ { 0.f, 0.f }, { 500.f, 1.f }, { 1000.f, 0.f } });
            case Wobble: return S::make ({ { 0.f, 0.f, 0.3f }, { 250.f, 0.5f, -0.3f }, { 500.f, 1.f, 0.3f }, { 750.f, 0.5f, -0.3f }, { 1000.f, 0.f } });
            case Steps:
            {
                const int n = juce::jlimit (2, 16, (int) std::round (beats * 4.0));
                S s; s.n = 0;
                for (int i = 0; i < n; ++i)
                {
                    const float v = stepLevel (i, seed);
                    s.pts[(size_t) s.n++] = { 1000.f * (float) i / (float) n, v };
                    s.pts[(size_t) s.n++] = { 1000.f * (float) (i + 1) / (float) n - (i + 1 < n ? 0.5f : 0.f), v };
                }
                s.tidy(); return s;
            }
            default:     return S::make ({ { 0.f, 0.f }, { 1000.f, 1.f } });       // Rise (also the default CUSTOM)
        }
    }
    inline doomsday::mod::Shape defaultCustom() { return toPoints (Rise, 1.0, 0); }

    // the value of MOD m at a beat position (custom: seg = eval cursor)
    inline float valueAt (int type, double beat, double len, int seed, const doomsday::mod::Shape& custom, int& seg)
    {
        if (type == Custom)
        {
            const double ph = beat / len - std::floor (beat / len);
            return juce::jlimit (0.f, 1.f, custom.eval ((float) ph * 1000.f, seg));
        }
        return builtin (type, beat, len, seed);
    }
}
