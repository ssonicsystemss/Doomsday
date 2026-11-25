#pragma once
// =====================================================================
//  4GE - FAST DIST STAGE (owner spec 2026-11-25 "FAST DIST STAGE", FourGE/FAST_DIST.md): a functional counterpart of
//  Fruity Fast Dist - NOT a bit-exact copy (its DSP is not public) and NOT a plain tanh drive.
//    INPUT -> PRE GAIN -> THRESHOLD-CONTROLLED NONLINEAR CORE (A / B) -> [ASYMMETRY] -> DC BLOCK -> MIX (dry / wet) -> POST
//   PRE  : how much goes INTO the distortion (0.5 = 1:1, 1 = +24 dB, 0 = -24 dB)
//   THRES: where (and how hard) the nonlinear region starts: lower = earlier + more harmonics + more aggression; below the
//          threshold the wave stays linear. The output is normalised to the curve's ceiling, so a low THRES also lifts the
//          quiet parts (the "intensity" half of the knob)
//   A    : dense / saturating - an exponential knee above THRES (fast, aggressive, harmonic, never noise)
//   B    : harder / clipping-like - a near-hard knee above THRES (a slight slope, then the ceiling)
//   MIX  : dry * (1 - mix) + wet * mix;  POST: the level after (never the curve)
//   ASYMMETRY (optional): + DRIVE / - DRIVE (dB) per half + BIAS, scaled by ASYMMETRY; the DC goes out after (5 Hz)
//  Pure DSP, deterministic, no allocation; runs at the engine's (oversampled) rate.
// =====================================================================
#include <cmath>
#include <algorithm>

namespace fge::dsp::fastdist
{
    struct Settings
    {
        bool on = false;
        float pre = 0.5f, thres = 1.f, mix = 1.f, postDb = 0.f;
        int mode = 0;                                              // 0 = A, 1 = B
        float asym = 0.f, posDb = 0.f, negDb = 0.f, bias = 0.f;    // (advanced)
    };

    // the core curve on |x| / threshold (pure: the tests call it). Returns the shaped value in -1 .. 1.
    inline float shape (float x, float thres, int mode)
    {
        const float t = std::clamp (thres, 0.02f, 1.f);
        const float z = std::abs (x) / t;
        float s, ceil;
        if (mode == 1) { s = z < 1.f ? z : 1.f + 0.08f * (z - 1.f); ceil = 1.25f; s = std::min (s, ceil); }   // B: hard knee
        else           { s = z < 1.f ? z : 1.f + 0.5f * (1.f - std::exp (-2.f * (z - 1.f))); ceil = 1.5f; } // A: dense knee
        return std::copysign (s / ceil, x);
    }

    class Stage
    {
    public:
        void prepare (double rate) { fs = rate; dcCoef = (float) std::exp (-2.0 * 3.14159265358979 * 5.0 / rate); reset(); }
        void reset() { dcX = dcY = 0.f; }
        // per block (cheap)
        void cook (const Settings& s)
        {
            preG = std::pow (10.f, (std::clamp (s.pre, 0.f, 1.f) - 0.5f) * 48.f / 20.f);
            postG = std::pow (10.f, s.postDb / 20.f);
            posG = std::pow (10.f, s.posDb / 20.f); negG = std::pow (10.f, s.negDb / 20.f);
        }
        float process (const Settings& s, float x)
        {
            if (! s.on) return x;
            const float dry = x;
            float v = x * preG;
            const bool asym = s.asym > 0.f;
            if (asym)
            {
                const float a = std::clamp (s.asym, 0.f, 1.f);
                v += a * s.bias * std::clamp (s.thres, 0.02f, 1.f);                       // BIAS: moves the wave against the threshold
                v *= v >= 0.f ? 1.f + a * (posG - 1.f) : 1.f + a * (negG - 1.f);         // + / - DRIVE per half
            }
            float wet = shape (v, s.thres, s.mode);
            if (asym) { const float y = wet - dcX + dcCoef * dcY; dcX = wet; dcY = y; wet = y; }   // DC BLOCK (only with asymmetry)
            const float m = std::clamp (s.mix, 0.f, 1.f);
            return (dry * (1.f - m) + wet * m) * postG;
        }
    private:
        double fs = 48000.0;
        float preG = 1.f, postG = 1.f, posG = 1.f, negG = 1.f, dcCoef = 0.999f, dcX = 0.f, dcY = 0.f;
    };
}
