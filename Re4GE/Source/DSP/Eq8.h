#pragma once
// RE4GE - POST EQ: 8 bands (after the tone chain + smear of each chain, BEFORE the clean SUB is added - so a LOW CUT here never
//   cuts the sub: the owner's rule, the low end stays). Types and filters as Mirror of Souls (Shared BiquadCoefs, the same
//   function draws the curve in the GUI): LOW CUT / LOW SHELF / BELL / NOTCH / HIGH SHELF / HIGH CUT (cuts 24 dB/oct Butterworth).
// + COMP (v0.13): a plain feed-forward compressor on the finished stereo bass (linked, soft knee), before CLIP / OUTPUT.
#include "ReParams.h"

namespace re
{
    enum EqType { EqLowCut = 0, EqLowShelf, EqBell, EqNotch, EqHighShelf, EqHighCut };

    struct EqCoefs
    {
        // per band up to two sections (the cuts); n = how many are used (0 = band off)
        std::array<std::array<doomsday::dsp::BiquadCoefs, 2>, 8> c {};
        std::array<int, 8> n {};
        bool any = false;
        // the same maths for the sound and the GUI curve
        static int make (const Params::EqBand& b, double fs, std::array<doomsday::dsp::BiquadCoefs, 2>& out)
        {
            using BC = doomsday::dsp::BiquadCoefs;
            if (! b.on) return 0;
            switch (b.type)
            {
                case EqLowCut:
                case EqHighCut:
                {
                    static const double qs[2] { 0.54119610, 1.30656296 };
                    for (int k = 0; k < 2; ++k) out[(size_t) k] = BC::make (b.type == EqLowCut ? BC::HighPass : BC::LowPass, fs, b.hz, qs[k]);
                    return 2;
                }
                case EqLowShelf:  out[0] = BC::make (BC::LowShelf, fs, b.hz, juce::jmax (0.3f, b.q), b.db); return 1;
                case EqHighShelf: out[0] = BC::make (BC::HighShelf, fs, b.hz, juce::jmax (0.3f, b.q), b.db); return 1;
                case EqNotch:     out[0] = BC::make (BC::Notch, fs, b.hz, b.q); return 1;
                default:          out[0] = BC::make (BC::Bell, fs, b.hz, b.q, b.db); return std::abs (b.db) > 0.01f ? 1 : 0;
            }
        }
        void cook (const Params& p, double fs)
        {
            any = false;
            for (int i = 0; i < 8; ++i) { n[(size_t) i] = make (p.eq[(size_t) i], fs, c[(size_t) i]); any |= n[(size_t) i] > 0; }
        }
        // the whole EQ's gain at hz (dB) - for the GUI curve
        static float responseDb (const Params& p, double hz, double fs)
        {
            double m = 1.0;
            for (int i = 0; i < 8; ++i)
            {
                std::array<doomsday::dsp::BiquadCoefs, 2> cc {};
                const int k = make (p.eq[(size_t) i], fs, cc);
                for (int s = 0; s < k; ++s) m *= cc[(size_t) s].magnitude (hz, fs);
            }
            return (float) (20.0 * std::log10 (juce::jmax (1.0e-6, m)));
        }
    };

    struct Eq8
    {
        std::array<std::array<doomsday::dsp::Biquad, 2>, 8> st {};
        void clear() { for (auto& b : st) for (auto& s : b) s.reset(); }
        inline float process (float x, const EqCoefs& c)
        {
            if (! c.any) return x;
            for (int i = 0; i < 8; ++i)
                for (int s = 0; s < c.n[(size_t) i]; ++s) x = st[(size_t) i][(size_t) s].process (x, c.c[(size_t) i][(size_t) s]);
            return x;
        }
    };

    // COMP: threshold / ratio / knee 6 dB / attack / release / makeup / mix; the gain reduction for the GUI
    struct Comp
    {
        float env = 0.f, grDb = 0.f, att = 0.f, rel = 0.f, makeup = 1.f;
        void clear() { env = 0.f; grDb = 0.f; }
        void cook (const Params& p, double fs)
        {
            att = (float) std::exp (-1.0 / (juce::jmax (0.05, (double) p.compAttackMs) * 0.001 * fs));
            rel = (float) std::exp (-1.0 / (juce::jmax (5.0, (double) p.compReleaseMs) * 0.001 * fs));
            makeup = std::pow (10.f, p.compMakeupDb / 20.f);
        }
        // returns the gain for this sample (linear), from the linked level
        inline float gain (float level, const Params& p)
        {
            env = level > env ? att * env + (1.f - att) * level : rel * env + (1.f - rel) * level;
            const float x = 20.f * std::log10 (juce::jmax (1.0e-6f, env));
            const float over = x - p.compThreshDb, k = 6.f, slope = 1.f / juce::jmax (1.f, p.compRatio) - 1.f;
            float g = 0.f;
            if (over > k * 0.5f) g = slope * over;
            else if (over > -k * 0.5f) g = slope * (over + k * 0.5f) * (over + k * 0.5f) / (2.f * k);
            grDb = g;
            const float wet = std::pow (10.f, g / 20.f) * makeup;
            return 1.f + p.compMix * (wet - 1.f);
        }
    };
}
