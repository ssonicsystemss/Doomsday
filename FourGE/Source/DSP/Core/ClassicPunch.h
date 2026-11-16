#pragma once
// =====================================================================
//  4GE - PUNCH TYPE CLASSIC (EARLY REWORK, owner 2026-11-16, FourGE/EARLY_REWORK.md): the old, hard, short hardstyle punch whose
//  character comes from the CONSTRUCTION of the attack - the hero of the first ~80-120 ms of an EARLY kick. Its own engine (rule 0).
//    1 SOURCE     a HYBRID, never a pure sine: a tonal oscillator (SHAPE sine -> triangle -> rounded saw) + a short TRANSIENT
//                 component (a fast-decaying knock above the punch) + a slightly RESONANT low-mid / mid (a band-pass ring excited by
//                 the source) + very controlled NOISE in the attack only. A hit, never a click.
//    2 PITCH      three stages (measured on the owner's 20 EARLY references, 2026-11-16: ~500-700 Hz at 2-5 ms -> ~230 Hz at ~12 ms
//                 -> ~100 Hz at ~60 ms -> the note by ~200 ms): START -> a fast HIT drop (DROP ms) -> a slower GLIDE to the
//                 FUNDAMENTAL (END = the note + END st, or END Hz with FOLLOW off) -> the body / tail take over
//    3 SPECTRUM   pre-EQ: LOW CUT, LOW-MID (the meat), MID (the character of the hit); post-EQ HIGH CUT (top only for the transient)
//    4 DRIVE      pre-EQ -> CHARACTER SATURATION -> WAVESHAPER -> post-EQ -> TRANSIENT CONTROL: the distortion brings out the
//                 harmonics of the existing punch, it does not create its character
//    5 SHAPE      ATTACK / HOLD / DECAY (+ curve) - short
//  Mono. DSP only; reset() per kick, no allocation.
// =====================================================================
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "../Tone/Biquad.h"

namespace fge::dsp::classicpunch
{
    struct Settings
    {
        bool on = false;
        // 1 source
        float shape = 0.35f, transLevel = 0.5f, transMs = 3.f, transHz = 1800.f, resHz = 420.f, resQ = 3.f, resLevel = 0.4f, noise = 0.15f, noiseHz = 3000.f;
        // 2 pitch
        float startHz = 650.f, hitHz = 230.f, dropMs = 10.f, glideMs = 60.f, curve = 0.6f, endSt = 12.f, endHz = 100.f; bool follow = true;
        // 3 spectrum
        float lowCut = 35.f, lowMidHz = 280.f, lowMidDb = 4.f, midHz = 1100.f, midDb = 5.f, highCut = 7000.f;
        // 4 drive
        float satDb = 8.f, asym = 0.2f, satDecayMs = 30.f; int wsType = 0; float wsDb = 6.f, transCtrl = 0.3f;   // (the drive fades with SAT DECAY: harmonics on the hit, a cleaner fundamental after)
        // 5 shape
        float attackMs = 0.5f, holdMs = 25.f, decayMs = 70.f, decayCurve = 0.5f;
    };

    class Unit
    {
    public:
        void prepare (double rate) { fs = rate; reset(); }
        void reset()
        {
            p = 0.0; pt = 0.0; blk = 0; seed = 0x51f15e5du; envF = envS = 0.f;
            for (auto* b : { &rs, &nb, &lc, &lm, &md, &hc1, &hc2, &dc }) b->clear();
        }

        float process (const Settings& s, float ms, float noteHz)
        {
            const float total = s.attackMs + s.holdMs + s.decayMs;
            if (! s.on || ms > total + 2.f) return 0.f;
            if (blk-- <= 0) { blk = 16; cook (s, ms, noteHz); }
            // ---- 1 SOURCE
            const double ph = p;
            const float sn = (float) std::sin (6.283185307 * ph), tri = (float) (4.0 * std::abs (ph - 0.5) - 1.0);
            const float rsaw = std::tanh (2.5f * (float) (2.0 * ph - 1.0)) / std::tanh (2.5f);
            const float sh = std::clamp (s.shape, 0.f, 1.f);
            float x = sh < 0.5f ? sn + (tri - sn) * sh * 2.f : tri + (rsaw - tri) * (sh - 0.5f) * 2.f;
            p += hz / fs; if (p >= 1.0) p -= 1.0;
            // the TRANSIENT component: a fast-decaying knock above the punch (its own short envelope)
            if (s.transLevel > 0.f && ms < s.transMs * 4.f)
            {
                pt += s.transHz * (1.0 + 0.5 * std::exp (-ms / 0.6)) / fs; if (pt >= 1.0) pt -= 1.0;
                x += s.transLevel * (float) std::sin (6.283185307 * pt) * std::exp (-ms / std::max (0.2f, s.transMs));
            }
            // the RESONANT low-mid / mid: a band-pass ring excited by the source
            x += s.resLevel * 2.f * rs.tick (x);
            // controlled NOISE, the attack only (band-limited)
            if (s.noise > 0.f && ms < 8.f) x += s.noise * nb.tick (noise()) * std::exp (-ms / 1.5f);
            // ---- 3 pre-EQ -> 4 CHARACTER SATURATION -> WAVESHAPER -> post-EQ -> TRANSIENT CONTROL
            x = md.tick (lm.tick (lc.tick (x)));
            x = sat (x * satG * satEnv, s.asym) * satComp;
            x = shaper (s.wsType, x * wsG * satEnv) * wsComp;
            x = dc.tick (hc2.tick (hc1.tick (x)));
            const float a = std::abs (x);
            envF += (a - envF) * (a > envF ? 0.2f : 0.02f) * (float) (176400.0 / fs);
            envS += (a - envS) * 0.0015f * (float) (176400.0 / fs);
            const float tg = std::clamp (1.f + s.transCtrl * (envF / std::max (1.0e-4f, envS) - 1.f) * 0.5f, 0.25f, 3.f);
            x *= tg;
            // ---- 5 SHAPE
            return x * ampEnv (s, ms) * 0.7f;
        }

        float lastPitchHz() const { return (float) hz; }

    private:
        static float sat (float x, float asym) { if (x < 0.f) x *= 1.f - 0.5f * std::clamp (asym, 0.f, 1.f); return std::tanh (x); }
        static float shaper (int type, float x)
        {
            switch (type)
            {
                case 1: return std::clamp (x, -1.f, 1.f);                                                 // HARD
                case 2: return x >= 0.f ? std::tanh (1.4f * x) : 0.65f * std::tanh (0.8f * x);             // ASYMMETRIC
                case 3: return x >= 0.f ? 1.f - std::exp (-1.2f * x) : -(1.f - std::exp (x)) * 0.9f;       // TUBE
                default: return std::tanh (x);                                                             // SOFT
            }
        }
        float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f); }
        static float ampEnv (const Settings& s, float ms)
        {
            if (ms < s.attackMs) return std::sin (1.5707963f * ms / std::max (0.05f, s.attackMs));
            const float t = ms - s.attackMs;
            if (t < s.holdMs) return 1.f;
            const float u = (t - s.holdMs) / std::max (1.f, s.decayMs);
            if (u >= 1.f) return 0.f;
            return std::pow (1.f - u, 0.6f + 3.f * (1.f - std::clamp (s.decayCurve, 0.f, 1.f)));
        }
        void cook (const Settings& s, float ms, float noteHz)
        {
            // 2 PITCH: START -> (fast HIT drop) -> HIT Hz -> (GLIDE) -> the FUNDAMENTAL
            const double end = s.follow && noteHz > 0.f ? noteHz * std::exp2 (s.endSt / 12.0) : (double) s.endHz;
            const double hit = std::max (end, (double) s.hitHz);
            const float k = 1.f + 3.f * s.curve;
            if (ms < s.dropMs)
            {
                const float u = ms / std::max (0.5f, s.dropMs);
                hz = hit * std::pow (std::max ((double) s.startHz, hit) / hit, (double) std::pow (1.f - u, k));
            }
            else
            {
                const float u = std::clamp ((ms - s.dropMs) / std::max (1.f, s.glideMs), 0.f, 1.f);
                hz = end * std::pow (hit / end, (double) std::pow (1.f - u, 0.5f + 0.5f * k));
            }
            hz = std::clamp (hz, 10.0, fs * 0.3);
            satEnv = 0.3f + 0.7f * std::exp (-ms / std::max (2.f, s.satDecayMs));
            rs.bp (fs, std::clamp ((double) s.resHz, 60.0, fs * 0.4), std::clamp ((double) s.resQ, 0.5, 20.0));
            nb.bp (fs, std::clamp ((double) s.noiseHz, 300.0, fs * 0.4), 1.2);
            lc.hp (fs, std::max (10.f, s.lowCut), 0.7071);
            lm.bell (fs, std::clamp ((double) s.lowMidHz, 80.0, 1000.0), s.lowMidDb, 1.0);
            md.bell (fs, std::clamp ((double) s.midHz, 300.0, 5000.0), s.midDb, 1.2);
            hc1.lp (fs, std::clamp ((double) s.highCut, 1000.0, fs * 0.45), 0.7071); hc2.lp (fs, std::clamp ((double) s.highCut, 1000.0, fs * 0.45), 0.7071);
            dc.hp (fs, 15.0, 0.7071);
            satG = std::pow (10.f, s.satDb / 20.f); satComp = 1.f / std::max (0.3f, std::tanh (satG * 0.5f) * 1.6f);
            wsG = std::pow (10.f, s.wsDb / 20.f); wsComp = 1.f / std::max (0.3f, std::tanh (wsG * 0.5f) * 1.6f);
        }

        double fs = 48000.0, p = 0.0, pt = 0.0, hz = 100.0;
        int blk = 0;
        uint32_t seed = 0x51f15e5du;
        float envF = 0.f, envS = 0.f, satEnv = 1.f, satG = 1.f, satComp = 1.f, wsG = 1.f, wsComp = 1.f;
        bq::Biquad rs, nb, lc, lm, md, hc1, hc2, dc;
    };
}
