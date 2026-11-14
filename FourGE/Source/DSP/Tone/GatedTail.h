#pragma once
// =====================================================================
//  4GE - TAIL ENGINE GATED (owner 2026-11-14, the PVC tail: "heavy gated bass tail"): on the TAIL group -
//  PRE-COMP (before the tail's distortion: an even, dense saturation), then AFTER the distortion a resonant low-pass whose
//  cutoff MOVES (an LFO) and the GATE: END = full level, one hard cut at GATE LENGTH; PATTERN = chopped in time with the song.
//  DSP only: per-block coefficients, no allocation. reset() at every new kick.
// =====================================================================
#include <cmath>
#include <algorithm>

namespace fge::dsp::gated
{
    struct Settings
    {
        bool on = false;
        float comp = 0.5f;
        int mode = 0;                 // 0 END, 1 PATTERN
        float gateMs = 380.f, depth = 0.9f;
        int div = 1;                  // 1/8, 1/16, 1/32
        float bpm = 150.f;
        float filterHz = 2000.f, reso = 2.f, moveHz = 6.f, moveDepth = 0.3f;
    };

    class Unit
    {
    public:
        void prepare (double rate) { fs = rate; reset(); }
        void reset() { env = 0.f; gs = 0.f; ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.f; lfo = 0.0; blk = 0; }
        // before the tail's distortion
        void preComp (const Settings& s, float& l, float& r)
        {
            const float a = std::abs (l) + std::abs (r);
            const float co = a > env ? atk : rel;
            env = co * env + (1.f - co) * a * 0.5f;
            const float g = std::min (8.f, std::pow (std::max (env, 1.0e-3f) / 0.5f, -s.comp));
            l *= g; r *= g;
        }
        // after the tail's distortion; ms = time since the hit
        void process (const Settings& s, float& l, float& r, float ms)
        {
            if (blk-- <= 0)                                                     // per 32 samples: the moving cutoff
            {
                blk = 32;
                const float f = std::clamp (s.filterHz * std::exp2 (s.moveDepth * 2.f * (float) std::sin (lfo)), 40.f, (float) fs * 0.45f);
                g1 = std::tan (3.14159265f * f / (float) fs); k1 = 1.f / std::max (0.5f, s.reso);
                a1 = 1.f / (1.f + g1 * (g1 + k1)); a2 = g1 * a1; a3 = g1 * a2;
            }
            lfo += 6.283185307 * s.moveHz / fs; if (lfo > 6.283185307) lfo -= 6.283185307;
            l = lp (0, l); r = lp (1, r);
            float g = ms < s.gateMs ? 1.f : 0.f;
            if (s.mode == 1)
            {
                static constexpr float divs[3] { 8.f, 16.f, 32.f };
                const float per = 60000.f / std::max (30.f, s.bpm) / (divs[std::clamp (s.div, 0, 2)] / 4.f);
                const float ph = std::fmod (ms, per) / per;
                if (ph >= 0.55f) g *= 1.f - s.depth;
            }
            gs += (g - gs) * sm;                                                // ~1 ms smoothing (no clicks)
            l *= gs; r *= gs;
        }
    private:
        float lp (int c, float x)
        {
            const float v3 = x - ic2[c], v1 = a1 * ic1[c] + a2 * v3, v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
            ic1[c] = 2.f * v1 - ic1[c]; ic2[c] = 2.f * v2 - ic2[c];
            return v2;
        }
        double fs = 48000.0, lfo = 0.0;
        float env = 0.f, gs = 0.f, ic1[2] {}, ic2[2] {};
        float g1 = 0.f, k1 = 1.f, a1 = 1.f, a2 = 0.f, a3 = 0.f;
        int blk = 0;
        float atk = 0.f, rel = 0.f, sm = 0.f;
    public:
        void setRate (double rate)
        {
            fs = rate;
            atk = std::exp (-1.f / (0.002f * (float) rate)); rel = std::exp (-1.f / (0.04f * (float) rate)); sm = 1.f - std::exp (-1.f / (0.001f * (float) rate));
            reset();
        }
    };
}
