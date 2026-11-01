#pragma once
// RE4GE - SYNTH source: SAW <-> PULSE (BLEND, WIDTH) + SINE, PHASE WARP (BEND / SQUEEZE / SYNC / MIRROR), unison detune pattern
#include "ReParams.h"
#include "DoomsdayDSP.h"
#include <cmath>

namespace re
{
    // PHASE WARP: bends the phase of the oscillator (0..1) -> other harmonics than drive, the 'springy' reverse bass
    inline double warp (double ph, int mode, double a)
    {
        a = juce::jlimit (0.0, 0.98, a);
        switch (mode)
        {
            case Bend:      { const double d = 0.5 * (1.0 - a); return ph < d ? 0.5 * ph / d : 0.5 + 0.5 * (ph - d) / (1.0 - d); }
            case Squeeze:   { const double half = ph < 0.5 ? 0.0 : 0.5, q = (ph - half) * 2.0, d = 1.0 - a * 0.9; return half + 0.5 * juce::jmin (1.0, q / d); }
            case Sync:      { const double r = 1.0 + a * 7.0, x = ph * r; return x - std::floor (x); }
            case Mirror:    { const double m = ph < 0.5 ? ph : 1.0 - ph; return ph + a * (m - ph); }
            default:        return ph;
        }
    }

    // the SYNTH wave at phase ph (warped by warpA), dt = phase step, sineG = linear SINE level
    inline double synthWave (const Params& p, double ph, double dt, double warpA, double sineG)
    {
        const double w = p.warp != WarpOff ? warp (ph, p.warp, warpA) : ph;
        double saw, pulse;
        if (p.warp == WarpOff)
        {
            saw = 2.0 * ph - 1.0 - doomsday::dsp::polyBlep ((float) ph, (float) dt);
            const double pw = juce::jlimit (0.05f, 0.95f, p.width);
            pulse = (ph < pw ? 1.0 : -1.0) + doomsday::dsp::polyBlep ((float) ph, (float) dt)
                  - doomsday::dsp::polyBlep (doomsday::dsp::wrap01 ((float) (ph + 1.0 - pw)), (float) dt) - (2.0 * pw - 1.0);
        }
        else
        {
            // warped: the shapes come from a sine of the bent phase (smooth, no hard edges to alias)
            saw = std::sin (juce::MathConstants<double>::twoPi * w);
            pulse = std::tanh (3.0 * std::sin (juce::MathConstants<double>::twoPi * w + (p.width - 0.5) * 2.0));
            if (p.warp == Sync) { const double win = 1.0 - ph; saw *= win; pulse *= win; }
        }
        const double sine = std::sin (juce::MathConstants<double>::twoPi * w);
        return (saw + (pulse - saw) * p.blend) * 0.7 + sine * sineG;
    }

    // detune (cents) of unison copy i of nu on side sd (1 = left, 2 = right; a different pattern per side) + its slow drift at tMs
    inline double uniCents (const Params& p, int i, int nu, int sd, double tMs)
    {
        const double spread = nu == 1 ? (sd == 1 ? -1.0 : 1.0) : (2.0 * i / (nu - 1) - 1.0) * (sd == 1 ? 1.0 : -0.83);
        const double rate = 0.4 + 0.31 * ((i * 7 + sd * 3) % 6), ph = 0.13 * (i + 1) * sd;
        return spread * p.uniDetuneCt + p.driftCt * std::sin (juce::MathConstants<double>::twoPi * (rate * tMs * 0.001 + ph));
    }
    // the per-side start phases of the unison copies (fixed: every step is the same)
    template <size_t N> inline void uniSeed (std::array<double, N>& uph, int sd)
    {
        uint32_t h = 0x9e3779b9u * (uint32_t) (sd + 1);
        for (auto& u : uph) { h ^= h << 13; h ^= h >> 17; h ^= h << 5; u = (double) (h & 0xffff) / 65536.0; }
    }

    // linear read of a buffer (0 outside), used for the kick tail and the sample
    inline float readBuf (const float* d, int len, double pos)
    {
        if (pos < 0.0 || pos >= len - 1) return 0.f;
        const int i = (int) pos; const float fr = (float) (pos - i);
        return d[i] + fr * (d[i + 1] - d[i]);
    }

    struct Sample { std::vector<float> data; double rate = 44100.0; juce::String name; };
}
