#pragma once
// =====================================================================
//  4GE - CLICK (owner 2026-11-16): a module of its own, the "ts" = the spark of the attack, 0 - 10 / 20 ms.
//  CLICK -> CLICK PROCESSING (drive / mix) -> the PUNCH BUS. It never goes through the ZAAG cascade (ZAAG would smear the
//  transient). Shared by every PUNCH TYPE; the type only changes HOW it is used (FrontProfiles.h).
//  TYPE: SOFT / HARD / DIGITAL / NOISE / METAL / PUNCHY. DSP only; reset() per kick, no allocation.
// =====================================================================
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "../Tone/Biquad.h"

namespace fge::dsp::front
{
    struct ClickSettings
    {
        bool on = false;
        int type = 1;                 // SOFT HARD DIGITAL NOISE METAL PUNCHY
        float gain = 0.f;             // linear (LEVEL x the profile)
        float pitchHz = 3000.f, lengthMs = 12.f, decay = 0.5f, tone = 0.6f, drive = 0.3f, mix = 1.f;
    };

    class ClickUnit
    {
    public:
        void prepare (double rate) { fs = rate; reset(); }
        void reset() { ph[0] = ph[1] = ph[2] = 0.0; seed = 0x51ed2701u; hold = 0.f; holdN = 0; lp.clear(); bp.clear(); blk = 0; }

        float process (const ClickSettings& s, float ms)
        {
            if (! s.on || s.gain <= 0.f || ms > s.lengthMs + 2.f) return 0.f;
            if (blk-- <= 0)
            {
                blk = 32;
                lp.lp (fs, std::clamp (200.0 * std::pow (2.0, 1.0 + s.tone * 6.5), 200.0, fs * 0.45), 0.7071);
                bp.bp (fs, std::clamp ((double) s.pitchHz, 100.0, fs * 0.4), 1.2 + 3.0 * (1.0 - s.tone));
            }
            const float len = std::max (0.5f, s.lengthMs), u = ms / len;
            // DECAY: 0 = held then cut hard, 1 = falls away at once (soft)
            const float k = 1.f + 9.f * s.decay;
            float env = std::exp (-k * u) * std::clamp ((len + 2.f - ms) * 0.5f, 0.f, 1.f);
            float x = 0.f;
            switch (s.type)
            {
                case 0: { env *= std::min (1.f, ms / 0.6f); x = osc (0, s.pitchHz * (1.f + 0.5f * std::exp (-ms / 1.5f))); break; }   // SOFT: rounded sine "tk"
                case 1: { x = 0.6f * osc (0, s.pitchHz) + 0.8f * noise() * std::exp (-ms / 0.7f); break; }                            // HARD: instant, bright "tik"
                case 2: { if (holdN-- <= 0) { holdN = 6; hold = std::round ((osc (0, s.pitchHz) + 0.5f * noise()) * 4.f) / 4.f; } x = hold; break; } // DIGITAL
                case 3: { x = bp.tick (noise()) * 2.5f; break; }                                                                       // NOISE
                case 4: { x = 0.5f * (osc (0, s.pitchHz) + osc (1, s.pitchHz * 1.483f) + 0.7f * osc (2, s.pitchHz * 2.137f)); break; }  // METAL
                default:                                                                                                              // PUNCHY: a saw diving down
                {
                    const double f = s.pitchHz * std::exp2 (-2.0 * std::min (1.f, u));
                    ph[0] += f / fs; if (ph[0] >= 1.0) ph[0] -= 1.0;
                    x = (float) (2.0 * ph[0] - 1.0); break;
                }
            }
            x = lp.tick (x * env);
            const float g = 1.f + 20.f * s.drive, wet = std::tanh (x * g) / std::tanh (g);
            return s.gain * (x + (wet - x) * s.mix);                                                 // MIX: clean <-> driven
        }
    private:
        float osc (int i, double hz) { ph[i] += hz / fs; if (ph[i] >= 1.0) ph[i] -= 1.0; return (float) std::sin (6.283185307 * ph[i]); }
        float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f); }
        double fs = 48000.0, ph[3] {};
        uint32_t seed = 0x51ed2701u;
        float hold = 0.f; int holdN = 0, blk = 0;
        bq::Biquad lp, bp;
    };
}
