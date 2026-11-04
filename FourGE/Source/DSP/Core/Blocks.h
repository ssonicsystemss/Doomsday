#pragma once
// =====================================================================
//  4GE core building blocks (module 01): oscillator, noise, the kick envelope, the basic pitch curve.
//  Pure DSP: no parameters, no GUI. Everything runs at the oversampled voice rate.
// =====================================================================
#include <juce_core/juce_core.h>
#include "DoomsdayDSP.h"
#include <cmath>
#include <cstdint>

namespace fge::dsp
{
    using doomsday::dsp::polyBlep;
    using doomsday::dsp::svfCoefs;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // waves: 0 SINE, 1 TRIANGLE, 2 SAW, 3 SQUARE, 4 PULSE (band-limited saw / square / pulse)
    enum Wave { Sine = 0, Triangle, Saw, Square, Pulse };

    struct Osc
    {
        float ph = 0.f;
        void reset (float phase01) { ph = phase01 - std::floor (phase01); }
        inline float tick (int wave, float hz, float inv, float pw = 0.5f)
        {
            const float dt = juce::jlimit (0.f, 0.45f, hz * inv);
            const float p = ph;
            ph += dt; if (ph >= 1.f) ph -= 1.f;
            switch (wave)
            {
                case Triangle: return 1.f - 4.f * std::abs (p - 0.5f) - 0.f;
                case Saw:      return 2.f * p - 1.f - polyBlep (p, dt);
                case Square:
                {
                    float s = (p < 0.5f ? 1.f : -1.f) + polyBlep (p, dt);
                    float q = p + 0.5f; if (q >= 1.f) q -= 1.f;
                    return s - polyBlep (q, dt);
                }
                case Pulse:
                {
                    const float w = juce::jlimit (0.05f, 0.95f, pw);
                    float s = (p < w ? 1.f : -1.f) + polyBlep (p, dt);
                    float q = p + 1.f - w; if (q >= 1.f) q -= 1.f;
                    return s - polyBlep (q, dt) - (2.f * w - 1.f);         // no DC
                }
                default:       return std::sin (twoPi * p);
            }
        }
    };

    // deterministic white noise (xorshift, the seed is reset on every kick)
    struct Noise
    {
        uint32_t s = 0x12345678u;
        void reset (uint32_t seed) { s = seed != 0 ? seed : 1u; }
        inline float tick() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.f / 2147483648.f); }
    };

    struct DcBlock
    {
        float x1 = 0.f, y1 = 0.f;
        inline float tick (float x, float r) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    };

    // ------------------------------------------------------------------ the kick envelope
    //  0 -> ATTACK (up to 1) -> DECAY (falls to SUSTAIN, exponential) -> holds SUSTAIN -> RELEASE (to 0 at LENGTH)
    //  If the release starts before the decay ends, it starts from wherever the decay is.
    struct EnvTimes { float attackMs = 0.f, decayMs = 100.f, sustain = 0.5f, releaseMs = 20.f, lengthMs = 300.f; };

    inline float kickEnv (const EnvTimes& e, float ms)
    {
        if (ms >= e.lengthMs || ms < 0.f) return 0.f;
        const float att = juce::jmax (0.15f, e.attackMs);                 // 0.15 ms minimum: no click at the start
        float v;
        if (ms < att) { const float x = ms / att; v = x * (2.f - x); }   // fast, slightly rounded rise
        else
        {
            const float x = (ms - att) / juce::jmax (0.5f, e.decayMs);
            const float d = x >= 1.f ? 0.f : (std::exp (-4.6f * x) - 0.01005f) / 0.98995f;
            v = e.sustain + (1.f - e.sustain) * d;
        }
        const float rel = juce::jmax (3.f, e.releaseMs);                // 3 ms minimum: no click at the end
        const float relStart = e.lengthMs - rel;
        if (ms > relStart)
        {
            const float x = (ms - relStart) / rel;
            v *= 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * juce::jlimit (0.f, 1.f, x));
        }
        return v;
    }

    // ------------------------------------------------------------------ the basic pitch curve (module 01)
    //  returns s in 0..1 = the fraction of the (log) distance START -> END still to go (1 at the hit, 0 landed)
    //  modes: 0 CLASSIC DROP, 1 FAST DROP, 2 SLOW DROP, 3 DOUBLE DROP, 4 RISE, 5 STABLE TONE
    inline float dropShape (float x, float curve)        // x 0..1 over the time, curve 0..1
    {
        if (x >= 1.f) return 0.f;
        if (x <= 0.f) return 1.f;
        if (curve <= 0.5f)
        {
            const float k = 0.05f + (0.5f - curve) * 2.f * 9.f;              // 0 = falls early and fast (exponential)
            return (std::exp (-k * x) - std::exp (-k)) / (1.f - std::exp (-k));
        }
        const float q = 1.f + (curve - 0.5f) * 2.f * 4.f;                    // 1 = stays up, falls late
        return 1.f - std::pow (x, q);
    }
    //  x = time / TIME (0..1 and beyond). Modes 0..7 are analytic; 8 CUSTOM is the drawn curve (env::Shape) - not here.
    inline float pitchShapeX (int mode, float x, float curve)
    {
        auto ease = [] (float u) { u = juce::jlimit (0.f, 1.f, u); return u * u * (3.f - 2.f * u); };
        switch (mode)
        {
            case 1:  return dropShape (x / 0.45f, juce::jmax (0.f, curve - 0.2f));                    // FAST: shorter, more convex
            case 2:  return dropShape (x / 2.2f, juce::jmin (1.f, curve + 0.2f));                     // SLOW: longer, gentler
            case 3:                                                                                  // DOUBLE: drop, short bounce up, drop
            {
                if (x < 0.3f)  return 0.35f + 0.65f * dropShape (x / 0.3f, curve);
                if (x < 0.42f) return 0.35f + 0.2f * ease ((x - 0.3f) / 0.12f);
                return 0.55f * dropShape ((x - 0.42f) / 0.58f, curve);
            }
            case 4:  return 1.f - dropShape (x, curve);                                             // RISE: END -> START
            case 5:  return 0.f;                                                                    // STABLE TONE
            case 6:                                                                                  // DROP + RISE
            {
                if (x < 0.5f) return dropShape (x / 0.5f, curve);
                return 0.35f * ease ((x - 0.5f) / 0.5f);
            }
            case 7:                                                                                  // HARDSTYLE KICK: fast transient, then the drop
            {
                if (x < 0.06f) return 0.55f + 0.45f * dropShape (x / 0.06f, 0.f);
                return 0.55f * dropShape ((x - 0.06f) / 0.94f, curve);
            }
            default: return dropShape (x, curve);                                                   // CLASSIC
        }
    }
    inline float pitchShape (int mode, float ms, float timeMs, float curve) { return pitchShapeX (mode, ms / juce::jmax (1.f, timeMs), curve); }
}
