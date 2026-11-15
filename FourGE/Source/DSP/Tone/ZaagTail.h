#pragma once
// =====================================================================
//  4GE - TAIL TYPE ZAAG (owner's spec 2026-11-14, On Point Samples 2026): the tail is the hero - a saturated saw shaped by
//  SERIAL stages, each one taking the harmonics of the one before:
//    SAW A + SAW B (oct / detune / level) -> PITCH (START st -> ROOT in DROP TIME, CURVE)
//    -> LOW CONTROL (how much low end goes into the distortion)
//    -> 01 GENERATE  (EQ: boost the fundamental, boost a mid, low cut -> waveshaper: drive / asym / curve)
//    -> 02 CHARACTER (EQ: low / low-mid / mid / high-mid -> waveshaper)
//    -> 03 DESTROY   (focus EQ -> dirt (noise, L / R differ = width) -> hard drive / asym; EXPERIMENTAL: bit reduction, fold)
//    -> FINAL EQ + SQUASH (more low end into the clipper) -> SOFT CLIPPER (clip / knee)
//    -> FILTER MOVEMENT (LP / BP, START -> END freq in TIME, curve, reso, LFO)
//    -> SUB SUPPORT (lows replaced by SAW / SINE / DERIVED, amount, crossover, decay; mono)
//    -> TAIL SHAPE (length - follows the kick length -, attack, body, decay, fade curve)
//  Macros: CHARACTER (CLEAN -> MILD SAW -> RAW -> HARSH -> LASER), GRIT, TONE, FOCUS. No punch here (rule 0: punch = PUNCH TYPE ZAAG).
//  DSP only: coefficients per 32 samples, no allocation; reset() per kick.
// =====================================================================
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "Biquad.h"

namespace fge::dsp::zaag
{
    struct Settings
    {
        bool on = false;
        // source + pitch
        float saw = 0.15f;                                       // saw A: sine <-> saw
        int sawbOct = 0; float sawbDetune = 7.f, sawbLevel = 0.4f;   // saw B: -1 / 0 / +1 octave, cents, level
        float startSt = 14.f, dropMs = 90.f, curve = 0.6f, rootSt = 0.f;
        float lowCtrl = 0.6f;
        // stages
        float s1Fund = 4.f, s1MidHz = 800.f, s1Mid = 3.f, s1LowCut = 10.f, s1Drive = 8.f, s1Asym = 0.f, s1Curve = 0.3f;
        float s2Low = 0.f, s2LowMid = 0.f, s2Mid = 0.f, s2HiMid = 0.f, s2Drive = 12.f;
        float driveDb = 28.f, s3Asym = 0.f, bits = 0.f, fold = 0.f, dirt = 0.09f, dirtHz = 9000.f;
        float squash = 0.5f, finalTone = 0.f, press = 3.f, knee = 0.5f;
        // filter movement
        int fltType = 0; float fltStart = 1500.f, fltEnd = 14000.f, fltTime = 250.f, fltCurve = 0.5f, fltReso = 1.f, fltLfo = 0.f, fltLfoHz = 5.f;
        // sub
        int subSrc = 1; float sub = 1.6f, subFreq = 120.f, subDecay = 2000.f;
        // macros
        float character = 0.5f, grit = 0.5f, tone = 0.f, focus = 0.5f;
        float width = 1.2f;
        // shape
        float lengthMs = 400.f, attackMs = 1.f, body = 0.7f, decay = 0.4f, fade = 0.5f;
    };

    class Unit
    {
    public:
        void prepare (double rate) { fs = rate; reset(); }
        void reset()
        {
            pa = pb = ps = 0.0; blk = 0; lfo = 0.0; seed = 0x1234567u;
            for (int c = 0; c < 2; ++c) for (auto* b : { &lowX[c], &hiX[c], &f1[c], &m1[c], &lc1[c], &e2a[c], &e2b[c], &e2c[c], &e2d[c], &foc[c], &nz[c], &sq[c], &tl[c], &mv[c], &hs1[c], &hs2[c] }) b->clear();
            for (auto* b : { &sb1, &sb2, &sd1, &sd2 }) b->clear();
        }
        void process (const Settings& s, float ms, float noteHz, float& outL, float& outR)
        {
            if (blk-- <= 0) { blk = 32; cook (s, ms, noteHz); }
            // ---- SOURCE: saw A + saw B at the falling pitch
            const float u = std::clamp (ms / std::max (1.f, s.dropMs), 0.f, 1.f);
            const float fall = std::pow (1.f - u, 1.f + 3.f * s.curve);              // CURVE: 0 linear .. 1 fast at the start
            const double root = (double) noteHz * std::exp2 (s.rootSt / 12.0);
            const double hz = std::clamp (root * std::exp2 (s.startSt * fall / 12.0), 5.0, fs * 0.4);
            const float sn = (float) std::sin (6.283185307 * pa), swa = (float) (2.0 * pa - 1.0), swb = (float) (2.0 * pb - 1.0);
            pa += hz / fs; if (pa >= 1.0) pa -= 1.0;
            pb += hz * bRatio / fs; if (pb >= 1.0) pb -= 1.0;
            const float src = ((1.f - s.saw) * sn + s.saw * swa + s.sawbLevel * swb) / (1.f + s.sawbLevel);
            float y[2];
            for (int c = 0; c < 2; ++c)
            {
                float x = src;
                // LOW CONTROL: how much low end goes into the distortion
                const float lo = lowX[c].tick (x);
                x = (x - lo) + s.lowCtrl * lo;
                // 01 GENERATE
                x = lc1[c].tick (m1[c].tick (f1[c].tick (x)));
                x = shape (x, g1, s.s1Asym + aAdd, s.s1Curve);
                // 02 CHARACTER
                x = e2d[c].tick (e2c[c].tick (e2b[c].tick (e2a[c].tick (x))));
                x = shape (x, g2, 0.5f * (s.s1Asym + aAdd), 0.2f);
                // 03 DESTROY (+ the dirt, different L / R)
                x = foc[c].tick (x) + s.dirt * nz[c].tick (noise());
                x = shape (x, g3, s.s3Asym + aAdd, 0.6f);
                if (s.fold > 0.f) x = (1.f - s.fold) * x + s.fold * std::sin (3.14159265f * (1.f + 2.f * s.fold) * x);
                if (s.bits > 0.f) { const float q = std::exp2 (8.f - 40.f * s.bits); x = std::round (x * q) / q; }
                // FINAL EQ + SQUASH -> SOFT CLIPPER
                x = tl[c].tick (sq[c].tick (x));
                x = softClip (x * clipGain, s.knee) * clipComp;
                // FILTER MOVEMENT
                x = mv[c].tick (x);
                y[c] = x;
            }
            // ---- SUB SUPPORT: the lows replaced by a controlled, mono sub
            const float mid = 0.5f * (y[0] + y[1]);
            const float hiL = hs2[0].tick (hs1[0].tick (y[0])), hiR = hs2[1].tick (hs1[1].tick (y[1]));
            float subS;
            if (s.subSrc == 0) subS = sb2.tick (sb1.tick (swa)) * 1.6f;                    // SAW: the raw saw's low end
            else if (s.subSrc == 1) subS = sn;                                              // SINE on the pitch
            else subS = sd2.tick (sd1.tick (mid));                                           // DERIVED: the processed low end
            subS *= s.sub * std::exp (-ms / std::max (10.f, s.subDecay));
            // ---- the highs wide (the dirt differs L / R), TAIL SHAPE
            const float hm = 0.5f * (hiL + hiR), hd = 0.5f * (hiL - hiR) * (1.f + 3.f * s.width);
            const float e = shapeEnv (s, ms);
            outL = (subS + hm + hd) * e;
            outR = (subS + hm - hd) * e;
        }
    private:
        static float shape (float x, float g, float asym, float curve)       // drive -> waveshaper (tanh <-> a harder knee), asymmetry
        {
            x *= g;
            if (x < 0.f) x *= 1.f - 0.6f * std::clamp (asym, 0.f, 1.f);
            const float t = std::tanh (x), h = std::clamp (x, -1.f, 1.f);
            return (1.f - curve) * t + curve * (0.5f * t + 0.5f * h);
        }
        static float softClip (float x, float knee)                         // soft clipper: linear below T, a smooth knee above
        {
            const float T = 1.f - 0.8f * std::clamp (knee, 0.f, 1.f), a = std::abs (x);
            if (a <= T) return x;
            const float k = 1.f - T;
            return std::copysign (T + k * std::tanh ((a - T) / std::max (1.0e-3f, k)), x);
        }
        float shapeEnv (const Settings& s, float ms) const
        {
            if (ms >= s.lengthMs) return 0.f;
            float e = s.attackMs > 0.01f ? std::min (1.f, ms / s.attackMs) : 1.f;
            const float u = ms / std::max (1.f, s.lengthMs), bodyEnd = std::clamp (s.body, 0.f, 1.f);
            if (u > bodyEnd)
            {
                const float v = (u - bodyEnd) / std::max (1.0e-3f, 1.f - bodyEnd);    // 0..1 over the rest
                const float level = 1.f - s.decay * std::pow (v, 0.5f + 2.f * (1.f - s.fade));
                e *= std::max (0.f, level);
            }
            const float left = s.lengthMs - ms;
            if (left < 3.f) e *= left / 3.f;                                          // no click at the end
            return e;
        }
        float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f) * 1.7f; }
        void cook (const Settings& s, float ms, float noteHz)
        {
            // CHARACTER: CLEAN -> MILD SAW -> RAW -> HARSH -> LASER (one knob, several things at once)
            static constexpr float kDrv[5] { 0.25f, 0.55f, 1.f, 1.35f, 1.7f }, kLow[5] { 3, 2, 0, -2, -4 }, kLM[5] { 2, 1, 0, -1, -3 },
                                   kMid[5] { 0, 1, 2, 3, 2 }, kHM[5] { -4, -1, 1, 4, 8 }, kA[5] { 0, 0, 0.1f, 0.2f, 0.1f }, kQ[5] { 0.7f, 0.8f, 1, 1.3f, 2.5f }, kCl[5] { 0.6f, 0.8f, 1, 1.2f, 1.3f };
            const float cp = std::clamp (s.character, 0.f, 1.f) * 4.f; const int ci = std::min (3, (int) cp); const float cf = cp - (float) ci;
            auto lerp = [&] (const float* t) { return t[ci] + (t[ci + 1] - t[ci]) * cf; };
            const float grit = 0.5f + s.grit, drv = lerp (kDrv) * grit;
            aAdd = lerp (kA);
            g1 = std::pow (10.f, s.s1Drive * drv / 20.f); g2 = std::pow (10.f, s.s2Drive * drv / 20.f); g3 = std::pow (10.f, s.driveDb * drv / 20.f);
            const double toneMul = std::exp2 ((double) s.tone);                          // TONE: every EQ an octave down / up
            const double q = (0.5 + 3.0 * s.focus) * lerp (kQ);                          // FOCUS: how narrow the EQs pick the harmonics
            const double root = std::max (20.0, (double) noteHz * std::exp2 (s.rootSt / 12.0));
            for (int c = 0; c < 2; ++c)
            {
                lowX[c].lp (fs, s.subFreq, 0.7071);
                f1[c].bell (fs, root, s.s1Fund, 1.0); m1[c].bell (fs, s.s1MidHz * toneMul, s.s1Mid, q); lc1[c].hp (fs, std::max (10.f, s.s1LowCut), 0.7071);
                e2a[c].shelf (fs, 120.0 * toneMul, s.s2Low + lerp (kLow), false);
                e2b[c].bell (fs, 350.0 * toneMul, s.s2LowMid + lerp (kLM), q);
                e2c[c].bell (fs, 1200.0 * toneMul, s.s2Mid + lerp (kMid), q);
                e2d[c].bell (fs, 3000.0 * toneMul, s.s2HiMid + lerp (kHM), q);
                foc[c].bell (fs, 1500.0 * toneMul, 3.0, q);
                nz[c].lp (fs, s.dirtHz, 0.7071);
                sq[c].shelf (fs, 120.0, 12.0 * s.squash, false);                         // SQUASH: more low end into the clipper
                tl[c].shelf (fs, 2500.0, s.finalTone * 4.0 + s.tone * 3.0, true);
                // FILTER MOVEMENT: START -> END in TIME (curve), + LFO
                const float v = std::clamp (ms / std::max (1.f, s.fltTime), 0.f, 1.f), w = std::pow (v, 0.3f + 2.f * s.fltCurve);
                double fc = s.fltStart * std::pow ((double) s.fltEnd / std::max (20.f, s.fltStart), (double) w);
                fc *= std::exp2 (s.fltLfo * 2.0 * std::sin (lfo));
                fc = std::clamp (fc, 30.0, fs * 0.45);
                if (s.fltType == 1) mv[c].bp (fs, fc, std::max (0.5f, s.fltReso)); else mv[c].lp (fs, fc, std::max (0.5f, s.fltReso) * 0.7071);
                hs1[c].hp (fs, s.subFreq, 0.7071); hs2[c].hp (fs, s.subFreq, 0.7071);
            }
            lfo += 6.283185307 * s.fltLfoHz * 32.0 / fs; if (lfo > 6.283185307) lfo -= 6.283185307;
            sb1.lp (fs, s.subFreq, 0.7071); sb2.lp (fs, s.subFreq, 0.7071); sd1.lp (fs, s.subFreq, 0.7071); sd2.lp (fs, s.subFreq, 0.7071);
            clipGain = s.press * lerp (kCl) * (1.f + 0.5f * s.squash);
            clipComp = 1.f / (1.f + 0.25f * s.squash);
            bRatio = std::exp2 ((double) s.sawbOct + s.sawbDetune / 1200.0);
        }
        double fs = 48000.0, pa = 0.0, pb = 0.0, ps = 0.0, lfo = 0.0, bRatio = 2.0;
        int blk = 0;
        uint32_t seed = 0x1234567u;
        float g1 = 1.f, g2 = 1.f, g3 = 1.f, aAdd = 0.f, clipGain = 1.f, clipComp = 1.f;
        bq::Biquad lowX[2], hiX[2], f1[2], m1[2], lc1[2], e2a[2], e2b[2], e2c[2], e2d[2], foc[2], nz[2], sq[2], tl[2], mv[2], hs1[2], hs2[2];
        bq::Biquad sb1, sb2, sd1, sd2;
    };
}
