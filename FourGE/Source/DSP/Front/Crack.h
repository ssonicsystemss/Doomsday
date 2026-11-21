#pragma once
// =====================================================================
//  4GE - RAW PUNCH ENGINE EXTENSION: CRACK SOURCE + SNAP + BODY COUPLING (owner 2026-11-21, FourGE/RAW_CRACK.md) - the RAW 2015-2017
//  "KRAK!", an independent layer (own source, own drive / clip / tone, own level), never a click, never more global distortion.
//  Only with PUNCH TYPE RAW (CLASSIC / CRACKING = its character); OFF = the RAW punch exactly as before.
//    SOURCE  ->  A. SNAP ENGINE (the physical first hit: its own short envelope, band, a low/mid BODY part, EDGE)
//            ->  B. CRACK SOURCE (its OWN harmonic tone: saw/square, CRACK PITCH = FOLLOW PUNCH (st above the RAW punch's own pitch)
//                   or FIXED (from a start down to the note) + PITCH MOD; + a little processed PUNCH, a transient burst, a noise texture)
//                   -> CRACK TONE (low / mid / high) -> CRACK DRIVE (+ type) + DENSITY folds + EDGE -> CRACK CLIP -> its own A/D/S/R
//                                                                                                     (A and B are parallel)
//            ->  BODY COUPLING (the crack's end synced to the BODY's start, it decays into it, the body's onset feeds its source;
//                   the body's own envelope is never touched, never a copy of the distorted body)
//            ->  RAW ATTACK BUS (SNAP + CRACK, own level, a soft peak ceiling) - mixed AFTER the master clip, so the kick's
//                   main DRIVE never touches the crack (distortion separation).
//  Runs at the engine's oversampled rate (the nonlinear stages are oversampled). DSP only; reset() per kick, no allocation.
// =====================================================================
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "../Tone/Biquad.h"

namespace fge::dsp::front
{
    struct CrackSettings
    {
        bool on = false;
        bool cracking = false;                // RAW CRACKING 2015-2017 (the RAW punch type): a harder character of the same layer
        float gain = 1.f;                     // linear (CRACK LEVEL)
        float snap = 0.6f, crack = 0.6f, density = 0.4f, edge = 0.5f, coupling = 0.6f;
        float snapLenMs = 4.f, snapTone = 0.6f, snapBody = 0.4f, snapEdge = 0.5f;
        int pitchMode = 0; float pitchSt = 19.f, fixedHz = 1200.f, pitchMod = 0.3f;
        float srcPunch = 0.3f, srcTrans = 0.4f, srcNoise = 0.1f;
        float attackMs = 0.3f, decayMs = 25.f, sustain = 0.05f, releaseMs = 20.f, lengthMs = 60.f;
        float driveDb = 14.f; int shape = 3; float clip = 0.5f, lowDb = -12.f, midDb = 3.f, highDb = 4.f;
        float bodyStartMs = 10.f;             // where the BODY comes in (its delay + attack): BODY COUPLING syncs the crack's end to it
    };

    class CrackUnit
    {
    public:
        void prepare (double rate) { fs = rate; reset(); }
        void reset()
        {
            p = 0.0; ps = 0.0; mph = 0.0; seed = 0x6a09e667u; blk = 0; shN = 0; sh = 0.f;
            for (auto* b : { &lc, &loSh, &midB, &hiSh, &slideLp, &snapBp, &edgeHp, &pre1, &cpLp }) b->clear();
        }
        float layerEndMs (const CrackSettings& s) const { return std::max (endMs (s) + s.releaseMs, s.snapLenMs * 3.f) + 60.f * s.coupling + 5.f; }
        // COUPLING: the crack's end is synced so it always reaches INTO the body's start (no hole between KRAK and the body)
        static float endMs (const CrackSettings& s) { const float sync = std::max (s.lengthMs, s.bodyStartMs + 1.5f * s.decayMs); return s.lengthMs + (sync - s.lengthMs) * std::clamp (s.coupling, 0.f, 1.f); }

        // punch = the RAW punch sample, punchHz = its pitch now, body = the BODY bus sample (raw), noteHz = the note
        float process (const CrackSettings& s, float ms, float punch, float punchHz, float body, float noteHz)
        {
            if (! s.on || s.gain <= 0.f || ms > layerEndMs (s)) return 0.f;
            const bool ck = s.cracking;
            if (blk-- <= 0) { blk = 32; cook (s, ms, noteHz, punchHz); }
            const float n = noise();
            // ---------------- A. SNAP ENGINE: a short decided impulse with a low/mid BODY part (never a bare click)
            float snapOut = 0.f;
            {
                const float len = std::max (0.5f, s.snapLenMs * (ck ? 0.7f : 1.f));
                const float atk = 0.05f + 0.6f * (1.f - s.snapEdge);
                const float env = ms < atk ? ms / atk : std::exp (-(ms - atk) / (len * 0.35f));
                ps += snapHz / fs; if (ps >= 1.0) ps -= 1.0;
                const float thump = (float) std::sin (6.283185307 * ps);                         // SNAP BODY: a fast falling low/mid thump
                const float hi = snapBp.tick (0.7f * n + (thump > 0.f ? 0.6f : -0.6f));          // SNAP TONE: the band of the hit
                float x = (s.snapBody * 1.2f * thump + (1.f - 0.5f * s.snapBody) * 1.4f * hi) * env;
                const float sat = 1.f + 6.f * s.snapEdge;                                         // SNAP EDGE: sharper = more saturated corner
                x = std::tanh (x * sat) / std::tanh (sat);
                snapOut = x * s.snap * (ck ? 1.35f : 1.f);
            }
            // ---------------- B. CRACK SOURCE: its own harmonic tone (not noise, not a click) + PITCH MOD
            float crackOut = 0.f;
            {
                mph += 70.0 / fs; if (mph >= 1.0) mph -= 1.0;
                if (++shN >= (int) (fs / 300.0)) { shN = 0; sh = noise(); }
                const double hzNow = toneNow * (1.0 + s.pitchMod * (0.12 * std::sin (6.283185307 * mph) + 0.08 * sh));
                const double ph = p; p += hzNow / fs; if (p >= 1.0) p -= 1.0;
                const float saw = (float) (2.0 * ph - 1.0), sq = ph < 0.5 ? 1.f : -1.f;
                const float tone = 0.6f * saw + 0.4f * sq;                                        // harmonics BEFORE any distortion
                const float trans = (ms < 4.f ? (float) std::exp (-ms / 0.8f) : 0.f) * (sq * 0.7f + 0.3f * n);
                const float cpl = s.coupling * std::clamp ((ms - 2.f) / 10.f, 0.f, 1.f);         // COUPLING: the body's onset feeds the crack source
                float x = tone + s.srcPunch * 1.5f * punch + s.srcTrans * trans + s.srcNoise * 0.5f * n + cpl * 0.8f * cpLp.tick (body);
                // CRACK TONE: low / mid / high (the crack's own; the low is cut so the punch and the body keep their room)
                x = hiSh.tick (midB.tick (loSh.tick (pre1.tick (x))));
                // CRACK DRIVE (x the CRACK macro) + its type, DENSITY folds
                x *= preG;
                switch (s.shape)
                {
                    case 0:  x = std::tanh (x); break;                                                         // SOFT
                    case 1:  x = std::clamp (x, -1.f, 1.f); break;                                             // HARD
                    case 2:  x = std::sin (x); break;                                                          // FOLD
                    default: x = x >= 0.f ? std::tanh (1.5f * x) : 0.75f * std::tanh (1.f * x); break;          // ASYM
                }
                for (int k = 0; k < folds; ++k) x = std::sin (x * (1.6f + 0.5f * (float) k));
                // EDGE: the upper harmonics, hard
                x += s.edge * (ck ? 0.8f : 0.5f) * std::clamp (edgeHp.tick (x) * (2.f + 8.f * s.edge), -1.f, 1.f);
                // CRACK CLIP (a sharp, cracking corner; the level kept)
                const float cl = 1.f - 0.6f * clipAmt;
                x = std::clamp (x, -cl, cl) / cl;
                // its own envelope (short; no sustain build-up); the top of the band slides down into the body (COUPLING)
                crackOut = slideLp.tick (lc.tick (x)) * crackEnv (s, ms) * crackLevel;
            }
            // ---------------- RAW ATTACK BUS: SNAP + CRACK, OUTPUT, a soft peak ceiling (never "louder = more aggressive")
            float y = (snapOut + crackOut) * s.gain * 0.5f;
            const float a = std::abs (y);
            if (a > 0.9f) y = std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f), y);
            return y;
        }

    private:
        float crackEnv (const CrackSettings& s, float ms) const
        {
            const float a = std::max (0.05f, s.attackMs);
            if (ms < a) return ms / a;
            const float d = std::max (2.f, s.decayMs * (1.f + 0.8f * s.coupling));             // COUPLING: the crack decays INTO the body
            const float t = ms - a;
            const float sus = std::clamp (s.sustain, 0.f, 0.3f);
            float e = sus + (1.f - sus) * std::exp (-t / (d * 0.4f));
            const float end = endMs (s);
            if (ms > end) e *= std::max (0.f, 1.f - (ms - end) / std::max (1.f, s.releaseMs));
            return e;
        }
        float noise() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (float) (int32_t) seed * (1.f / 2147483648.f); }
        void cook (const CrackSettings& s, float ms, float noteHz, float punchHz)
        {
            const bool ck = s.cracking;
            // CRACK PITCH: FOLLOW PUNCH (st against the RAW punch's own pitch) or FIXED (from FIXED PITCH down to ~4 x the note)
            const double note = (double) (noteHz > 0.f ? noteHz : 49.f);
            if (s.pitchMode == 0 && punchHz > 0.f) toneNow = std::clamp ((double) punchHz * std::exp2 (s.pitchSt / 12.0), 40.0, fs * 0.2);
            else
            {
                const double end = std::max (60.0, note * 4.0), st = std::max (end, (double) s.fixedHz);
                const float u = std::clamp (ms / (ck ? 18.f : 28.f), 0.f, 1.f);
                toneNow = end * std::pow (st / end, (double) (1.f - u) * (1.f - u));
            }
            snapHz = std::clamp (400.0 + 600.0 * s.snapTone, 80.0, fs * 0.3) * (1.0 - 0.6 * std::clamp (ms / std::max (1.f, s.snapLenMs), 0.f, 1.f));
            // the macros: CRACK = harmonics + drive + clip + level (not the kick's drive); CRACKING = a harder character
            const float cr = std::clamp (s.crack, 0.f, 1.f);
            preG = std::pow (10.f, (s.driveDb * (0.35f + (ck ? 1.4f : 1.f) * cr)) / 20.f);
            clipAmt = std::clamp (s.clip * (0.4f + (ck ? 1.f : 0.6f) * cr), 0.f, 0.95f);
            crackLevel = cr * (ck ? 1.2f : 1.f);
            folds = std::clamp ((int) std::lround ((ck ? 4.f : 3.f) * s.density), 0, 4);
            pre1.hp (fs, 80.0, 0.7071);
            loSh.shelf (fs, 350.0, s.lowDb, false); midB.bell (fs, 1600.0, s.midDb, 0.8); hiSh.shelf (fs, 4500.0, s.highDb, true);
            lc.hp (fs, 150.0, 0.7071);                                                           // the crack's lows controlled (the body keeps its mass)
            edgeHp.hp (fs, 3500.0, 0.7071);
            // COUPLING: the top of the crack slides down toward the body as it reaches the body's start
            const double slide = 1.0 - 0.75 * s.coupling * std::clamp (ms / std::max (5.f, endMs (s)), 0.f, 1.f);
            slideLp.lp (fs, std::clamp (16000.0 * slide, 900.0, fs * 0.45), 0.7071);
            snapBp.bp (fs, std::clamp (1500.0 * std::pow (2.0, 2.5 * s.snapTone), 500.0, fs * 0.4), 1.0);
            cpLp.lp (fs, 1500.0, 0.7071);
        }

        double fs = 48000.0, p = 0.0, ps = 0.0, mph = 0.0, toneNow = 1000.0, snapHz = 400.0;
        uint32_t seed = 0x6a09e667u;
        int blk = 0, folds = 1, shN = 0;
        float preG = 1.f, clipAmt = 0.f, crackLevel = 0.f, sh = 0.f;
        bq::Biquad lc, loSh, midB, hiSh, slideLp, snapBp, edgeHp, pre1, cpLp;
    };
}
