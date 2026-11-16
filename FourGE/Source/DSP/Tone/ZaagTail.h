#pragma once
// =====================================================================
//  4GE - TAIL TYPE ZAAG = the ZAAG ENGINE (owner's tutorial, 2026-11-15; spec: FourGE/ZAAG_ENGINE.md).
//  A CASCADE of dependent nonlinear processes - every stage changes the signal for the next one, no level matching in between:
//    01 SOURCE       LOW SAW (the note) + HIGH SAW (3 octaves up, quiet) - fixed, only HIGH SAW LEVEL
//    02 FUNDAMENTAL  the current pitch from the pitch drop (START st -> ROOT in DROP ms) = the frequency every EQ follows
//    EQ1             dynamic bell on the fundamental + a light mid boost      -> DIST 1 (maximally aggressive, mix)
//    EQ2             MOVING LOW SHELF: 0 -> 100 % over the punch, 100 % -> MOTION DEPTH over the tail -> DIST 2
//    EQ3             RESONANT SWEEP: a narrow peak locked to the kick envelope (high -> plateau -> second fall, optional reopen)
//    UNIQUE FX       ZAAG RESONATOR (PHASER / RESONATOR / COMB, x fundamental, stereo width, low mix; position selectable)
//    DIST 3          the main ZAAG CHARACTER waveshaper (SOFT blended into the TYPE; preamp, asymmetry, mix)
//    EQ4             SUB PUSH (low shelf into the clipper), LOW-MID CUT, HIGH ENHANCE -> SOFT CLIP (ZAAG CRUSH)
//    ZAAG STEREO     M / S: side high-pass (the lows ALWAYS mono) -> side boost -> width -> high reverb (top band only)
//    TAIL SHAPE      attack / body / decay / fade, then the SUB (its own envelope, mono) is added after the shape
//  KICK-LENGTH SYNC: every movement (shelf, sweep, resonator, distortion envelope) is read at pos = ms / KICK LENGTH, never in ms
//  (only the pitch drop - the hit - stays in ms). Macros: ZAAG, DRIVE, RESONANCE, MOVEMENT, BASS, STEREO, CRUSH, CHARACTER.
//  Variants: CLASSIC / RESONANT / BOUNCE / AGGRESSIVE = internal recipe constants. No punch here (rule 0).
//  DSP only: buffers in prepare(), coefficients per 32 samples, no allocation; reset() per kick.
// =====================================================================
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>
#include "Biquad.h"
#include "Fdn.h"

namespace fge::dsp::zaag
{
    struct Settings
    {
        bool on = false;
        int variant = 0;                                                         // CLASSIC / RESONANT / BOUNCE / AGGRESSIVE
        // 01 source + 02 pitch (the drop is the hit: in ms)
        float highSaw = 0.12f, startSt = 12.f, dropMs = 100.f, curve = 0.6f, rootSt = 0.f;
        // EQ1 + DIST 1
        float lowCtrl = 0.6f, fundDb = 8.f, midHz = 800.f, midDb = 2.5f, lowCut = 10.f, d1Db = 20.f, d1Asym = 0.f, d1Hard = 0.8f, d1Mix = 1.f;
        // EQ2 (moving low shelf) + DIST 2
        float shelfDb = 9.f, motionDepth = 0.5f, motionTime = 0.18f, d2Db = 10.f;
        // EQ3 resonant sweep
        float swDb = 15.f, swQ = 8.f, swStart = 16.f, swPlateau = 6.f, swEnd = 2.f, swHold = 0.82f, swReopen = 0.f;
        // unique FX: resonator
        int rsnMode = 0, rsnPos = 2; float rsnFreq = 4.f, rsnReso = 0.5f, rsnWidth = 0.5f, rsnMix = 0.25f;
        // DIST 3
        int d3Type = 1; float d3Db = 12.f, d3Preamp = 0.f, d3Asym = 0.f, d3Mix = 1.f, distEnv = 0.3f, dirt = 0.09f, dirtHz = 9000.f;
        // EQ4 + soft clip
        float subPush = 0.2f, lmCut = 3.f, highEnh = 0.5f, press = 1.5f, knee = 0.5f;
        // stereo
        float sideCut = 180.f, sideBoostHz = 3000.f, sideBoost = 0.f, width = 0.8f, hverb = 0.2f;
        // sub
        int subSrc = 1; float sub = 1.4f, subFreq = 130.f, subDecay = 2000.f;
        // macros (0.5 = neutral)
        float zaag = 0.5f, drive = 0.5f, resonance = 0.5f, movement = 0.5f, bass = 0.5f, stereo = 0.5f, crush = 0.5f, character = 0.5f;
        // tail shape (length = KICK LENGTH)
        float lengthMs = 400.f, attackMs = 1.f, body = 0.7f, decay = 0.4f, fade = 0.5f;
    };

    // ---- the shape of every movement, read at pos = ms / KICK LENGTH (pure functions: the tests call them too)
    // the moving low shelf: 0 -> 1 over the punch (0..T), 1 -> depth over the tail; BOUNCE adds breaths in the tail
    inline float shelfMotion (float pos, float T, float depth, float movement, int variant)
    {
        pos = std::clamp (pos, 0.f, 1.f); T = std::clamp (T, 0.02f, 0.9f);
        float m = pos < T ? std::sin (1.5707963f * pos / T) : 1.f + (depth - 1.f) * (pos - T) / (1.f - T);
        if (variant == 2 && pos > T) { const float v = (pos - T) / (1.f - T); m *= 1.f - 0.55f * (0.5f - 0.5f * std::cos (6.2831853f * 3.f * v)); }
        const float amt = std::clamp (movement * 2.f, 0.f, 2.f);                   // MOVEMENT: 0 = static at 100 %, 0.5 = as designed, 1 = twice as deep
        return std::clamp (1.f - amt * (1.f - m), 0.f, 1.f);
    }
    // the resonant sweep (x the fundamental): START -> PLATEAU over the punch, hold, -> END after HOLD; REOPEN rises again at the end
    inline float sweepRatio (float pos, float T, float hold, float start, float plateau, float end, float reopen, float movement)
    {
        pos = std::clamp (pos, 0.f, 1.f); T = std::clamp (T, 0.02f, 0.9f); hold = std::clamp (hold, T + 0.01f, 1.f);
        const float k = std::clamp (movement * 2.f, 0.f, 2.f);                     // MOVEMENT: the travel (in octaves) around the plateau
        auto oct = [&] (float r) { return std::log2 (std::max (0.5f, r) / std::max (0.5f, plateau)) * k; };
        float o;
        if (pos < T) { const float u = pos / T; o = oct (start) * (1.f - u) * (1.f - u); }
        else if (pos < hold) o = 0.f;
        else { const float u = (pos - hold) / std::max (0.01f, 1.f - hold); o = oct (end) * u; }
        if (reopen > 0.f && pos > hold) { const float u = (pos - hold) / std::max (0.01f, 1.f - hold); o += reopen * std::abs (oct (start)) * u * u; }
        return std::max (0.5f, plateau) * std::exp2 (o);
    }

    class Unit
    {
    public:
        void prepare (double rate)
        {
            fs = rate; rev.prepare (rate, 1.5, 40.0);
            for (auto& c : comb) c.assign ((size_t) (rate / 15.0) + 8, 0.f);
            reset();
        }
        void reset()
        {
            pa = pb = 0.0; blk = 0; rev.reset(); seed = 0x1234567u;
            for (int c = 0; c < 2; ++c)
            {
                for (auto* b : { &lowX[c], &f1[c], &m1[c], &lc1[c], &sh2[c], &pk3[c], &rbp[c], &push[c], &lmc[c], &hie[c], &xo1[c], &xo2[c], &nz[c] }) b->clear();
                for (auto& a : ap[c]) a = 0.f;
                std::fill (comb[c].begin(), comb[c].end(), 0.f); combIdx[c] = 0; phFb[c] = 0.f; hold3[c] = 0.f; holdN[c] = 0;
            }
            for (auto* b : { &sc1, &sc2, &sc3, &sc4, &sbst, &hv1, &hv2, &sb1, &sb2, &sd1, &sd2 }) b->clear();
        }

        void process (const Settings& s, float ms, float noteHz, float& outL, float& outR, float ext = 0.f)   // ext = ZAAG SEND (TOK + PUNCH)
        {
            if (blk-- <= 0) { blk = 32; cook (s, ms, noteHz); }
            // ---- 01 SOURCE: LOW SAW at the falling pitch + HIGH SAW 3 octaves up
            const float sn = (float) std::sin (6.283185307 * pa), lo = (float) (2.0 * pa - 1.0), hi = (float) (2.0 * pb - 1.0);
            pa += hz / fs; if (pa >= 1.0) pa -= 1.0;
            pb += hz * 8.0 / fs; if (pb >= 1.0) pb -= 1.0;
            const float src = (lo + s.highSaw * hi) / (1.f + 0.5f * s.highSaw) + ext;
            float y[2];
            for (int c = 0; c < 2; ++c)
            {
                float x = src;
                const float low = lowX[c].tick (x);                                  // how much low end goes into the cascade (BASS MANAGEMENT)
                x = (x - low) + lowIn * low;
                if (s.rsnPos == 0) x = resonate (x, c, s);
                // EQ1 (dynamic bell on the fundamental + mid) -> DIST 1 (maximally aggressive)
                x = lc1[c].tick (m1[c].tick (f1[c].tick (x)));
                x = mix (x, shape (x * g1, d1Asym, s.d1Hard), s.d1Mix);
                if (s.rsnPos == 1) x = resonate (x, c, s);
                // EQ2 (moving low shelf) -> DIST 2
                x = sh2[c].tick (x);
                x = shape (x * g2, 0.5f * d1Asym, 0.4f);
                // EQ3 (resonant sweep) -> UNIQUE FX -> DIST 3 (the main character)
                x = pk3[c].tick (x);
                if (s.rsnPos == 2) x = resonate (x, c, s);
                x = mix (x, dist3 (x * g3, c, s), s.d3Mix);
                x += dirtAmt * nz[c].tick (noise()) * (0.2f + std::abs (x));       // DIRT: fizz riding the distorted signal (the OPS dirt to 10 kHz; L / R differ)
                if (s.rsnPos == 3) x = resonate (x, c, s);
                // EQ4 (SUB PUSH, LOW-MID CUT, HIGH ENHANCE) -> SOFT CLIP
                x = hie[c].tick (lmc[c].tick (push[c].tick (x)));
                x = softClip (x * clipGain, knee) * clipComp;
                y[c] = x;
            }
            // ---- ZAAG STEREO: M / S, the side high-passed (lows mono), side boost, width, high reverb
            float m = 0.5f * (y[0] + y[1]), sd = 0.5f * (y[0] - y[1]);
            sd = sbst.tick (sc4.tick (sc3.tick (sc2.tick (sc1.tick (sd))))) * sideGain;      // 48 dB / oct: the lows never on the side
            if (hvAmt > 0.f)
            {
                float rl = 0.f, rr = 0.f;
                rev.tick (hv2.tick (hv1.tick (m)), rl, rr);
                m += hvAmt * 0.3f * (rl + rr); sd += hvAmt * 1.2f * (rl - rr);
            }
            // ---- the lows replaced by a clean SUB (mono, after the tail shape, its own envelope)
            const float mHi = xo2[0].tick (xo1[0].tick (m)), sHi = xo2[1].tick (xo1[1].tick (sd));
            float subS;
            if (s.subSrc == 0) subS = sb2.tick (sb1.tick (lo)) * 1.6f;
            else if (s.subSrc == 1) subS = sn;
            else subS = sd2.tick (sd1.tick (m));
            const float e = shapeEnv (s, ms);
            float se = subGain * std::exp (-ms / std::max (10.f, s.subDecay));
            const float left = s.lengthMs - ms;
            if (left < 4.f) se *= std::max (0.f, left / 4.f);
            subS *= se * (1.f - 0.6f * subDuck);                                       // BOUNCE: the sub breathes with the shelf
            outL = ((mHi + sHi) * e * outGain + subS) * 0.5f;                       // (-6 dB: the MASTER CLIP squeezes it to 0 dB)
            outR = ((mHi - sHi) * e * outGain + subS) * 0.5f;
        }

        // (tests) the current movement state
        float lastShelf() const { return shelfNow; }
        float lastSweepHz() const { return sweepHzNow; }

    private:
        float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f) * 1.7f; }
        static float mix (float dry, float wet, float m) { return dry + (wet - dry) * std::clamp (m, 0.f, 1.f); }
        static float shape (float x, float asym, float hard)            // drive already applied: tanh <-> hard clip, asymmetry
        {
            if (x < 0.f) x *= 1.f - 0.6f * std::clamp (asym, 0.f, 1.f);
            const float t = std::tanh (x), h = std::clamp (x, -1.f, 1.f);
            return t + (h - t) * std::clamp (hard, 0.f, 1.f);
        }
        static float softClip (float x, float knee)
        {
            const float T = 1.f - 0.8f * std::clamp (knee, 0.f, 1.f), a = std::abs (x);
            if (a <= T) return x;
            const float k = 1.f - T;
            return std::copysign (T + k * std::tanh ((a - T) / std::max (1.0e-3f, k)), x);
        }
        float algo (int type, float x, int c)                            // the DISTORTION TYPEs of the main waveshaper
        {
            switch (type)
            {
                case 0: return std::tanh (x);                                                         // SOFT
                case 1: return std::clamp (x, -1.f, 1.f);                                             // HARD
                case 2: return std::sin (1.5707963f * std::tanh (1.3f * x));                             // WAVE
                case 3: return std::sin (1.5707963f * x);                                             // FOLD (keeps folding)
                case 4: return x >= 0.f ? std::tanh (1.6f * x) : 0.55f * std::tanh (0.7f * x);         // ASYMMETRIC
                case 5:                                                                               // DIGITAL: bits + sample hold
                {
                    if (holdN[c]-- <= 0) { holdN[c] = 3; const float q = 12.f; hold3[c] = std::round (std::clamp (x, -1.f, 1.f) * q) / q; }
                    return hold3[c];
                }
                default: { const float a = std::abs (x); return std::copysign (std::clamp ((a - 0.25f) * 8.f, 0.f, 1.f), x); }   // ZERO-SQUARE
            }
        }
        float dist3 (float x, int c, const Settings& s)
        {
            if (x < 0.f) x *= 1.f - 0.6f * std::clamp (s.d3Asym, 0.f, 1.f);
            return mix (std::tanh (x), algo (d3Type, x, c), charBlend);       // CHARACTER: SOFT -> the TYPE
        }
        float resonate (float x, int c, const Settings& s)               // ZAAG RESONATOR (adds character, low mix)
        {
            float w = x;
            if (s.rsnMode == 0)                                          // PHASER: 4 allpasses with feedback
            {
                float v = x + phFb[c] * rsnFb;
                for (auto& z : ap[c]) { const float o = apA[c] * v + z; z = v - apA[c] * o; v = o; }
                phFb[c] = v; w = 0.5f * (x + v);
            }
            else if (s.rsnMode == 1) w = x + rsnK * rbp[c].tick (x);     // RESONATOR: a narrow ringing peak
            else                                                         // COMB: negative feedback = hollow
            {
                auto& b = comb[c]; const int n = (int) b.size();
                int r = combIdx[c] - combLen[c]; if (r < 0) r += n;
                const float d = b[(size_t) r];
                w = x - rsnFb * d;
                b[(size_t) combIdx[c]] = w; if (++combIdx[c] >= n) combIdx[c] = 0;
                w = 0.6f * (x + w);
            }
            return x + (w - x) * rsnMixNow;
        }
        float shapeEnv (const Settings& s, float ms) const
        {
            if (ms >= s.lengthMs) return 0.f;
            float e = s.attackMs > 0.01f ? std::min (1.f, ms / s.attackMs) : 1.f;
            const float u = ms / std::max (1.f, s.lengthMs), bodyEnd = std::clamp (s.body, 0.f, 1.f);
            if (u > bodyEnd)
            {
                const float v = (u - bodyEnd) / std::max (1.0e-3f, 1.f - bodyEnd);
                e *= std::max (0.f, 1.f - s.decay * std::pow (v, 0.5f + 2.f * (1.f - s.fade)));
            }
            const float left = s.lengthMs - ms;
            if (left < 3.f) e *= left / 3.f;
            return e;
        }
        void cook (const Settings& s, float ms, float noteHz)
        {
            // variant recipe constants: sweep dB / Q, drive dB, character push, crush push, pitch hit
            static constexpr float vSw[4] { 0, 6, 0, 2 }, vQ[4] { 1, 1.6f, 1, 1 }, vDrv[4] { 0, 0, 2, 6 }, vChar[4] { 0, 0, 0, 0.3f }, vCrush[4] { 0, 0, 0.1f, 0.25f },
                                   vHit[4] { 0, 0, 4, 2 }, vReopen[4] { 0, 0.35f, 0, 0 };
            const int v = std::clamp (s.variant, 0, 3);
            const float L = std::max (20.f, s.lengthMs), pos = std::clamp (ms / L, 0.f, 1.f);
            // 02 FUNDAMENTAL: the pitch drop (ms) -> the frequency every EQ follows
            const float u = std::clamp (ms / std::max (1.f, s.dropMs), 0.f, 1.f);
            const float fall = std::pow (1.f - u, 1.f + 3.f * s.curve);
            const double root = std::max (10.0, (double) noteHz * std::exp2 (s.rootSt / 12.0));
            hz = std::clamp (root * std::exp2 ((s.startSt + vHit[v]) * fall / 12.0), 5.0, fs * 0.3);
            const double f0 = hz;
            // macros
            const float z = s.zaag - 0.5f;                                        // ZAAG: everything at once
            const float drvDb = (s.drive - 0.5f) * 18.f + z * 12.f + vDrv[v];      // DRIVE (+ ZAAG)
            const float env = 1.f - std::clamp (s.distEnv, 0.f, 1.f) * 0.5f * pos;   // DISTORTION ENVELOPE (KICK LENGTH)
            g1 = std::pow (10.f, (s.d1Db + drvDb) * env / 20.f);
            g2 = std::pow (10.f, (s.d2Db + drvDb) * env / 20.f);
            g3 = std::pow (10.f, (s.d3Db + s.d3Preamp + drvDb) * env / 20.f);
            d1Asym = s.d1Asym; d3Type = std::clamp (s.d3Type, 0, 6);
            charBlend = std::clamp (s.character + vChar[v], 0.f, 1.f);
            const float b = s.bass - 0.5f;                                        // BASS: sub <-> mids
            lowIn = std::clamp (s.lowCtrl * (1.f - 1.2f * b), 0.f, 1.5f);
            // EQ curves (KICK-LENGTH SYNC)
            shelfNow = shelfMotion (pos, s.motionTime, s.motionDepth, s.movement, v);
            const float swGain = std::clamp ((s.swDb + vSw[v]) * (0.4f + 1.2f * s.resonance) * (1.f + 1.2f * z), 0.f, 30.f);
            const double swQ = std::clamp ((double) s.swQ * vQ[v] * (0.5 + s.resonance), 0.7, 40.0);
            const float ratio = sweepRatio (pos, s.motionTime, s.swHold, s.swStart, s.swPlateau, s.swEnd, std::clamp (s.swReopen + vReopen[v], 0.f, 1.f), s.movement);
            sweepHzNow = (float) std::clamp (f0 * ratio, 30.0, fs * 0.4);
            for (int c = 0; c < 2; ++c)
            {
                lowX[c].lp (fs, s.subFreq, 0.7071);
                f1[c].bell (fs, std::clamp (f0, 20.0, fs * 0.4), s.fundDb + z * 6.f, 1.2);
                m1[c].bell (fs, s.midHz, s.midDb + 4.f * b, 0.9);
                lc1[c].hp (fs, std::max (10.f, s.lowCut), 0.7071);
                sh2[c].shelf (fs, 150.0, s.shelfDb * shelfNow, false);
                pk3[c].bell (fs, sweepHzNow, swGain, swQ);
                push[c].shelf (fs, 120.0, 12.f * std::clamp (s.subPush - 0.6f * b, 0.f, 1.5f), false);
                lmc[c].bell (fs, 320.0, -s.lmCut, 1.0);
                hie[c].shelf (fs, 2500.0, s.highEnh * 6.f, true);
                xo1[c].hp (fs, s.subFreq, 0.7071); xo2[c].hp (fs, s.subFreq, 0.7071);
                nz[c].lp (fs, s.dirtHz, 0.7071);
                // the resonator follows the fundamental and the sweep (KICK-LENGTH SYNC); L / R differ by its width
                const double rf = std::clamp (f0 * s.rsnFreq * std::sqrt (ratio / std::max (0.5f, s.swPlateau)) * (1.0 + (c ? 0.12 : -0.12) * s.rsnWidth), 30.0, fs * 0.4);
                const double t = std::tan (3.14159265 * std::min (rf, fs * 0.45) / fs);
                apA[c] = (float) ((t - 1.0) / (t + 1.0));
                rbp[c].bp (fs, rf, 2.0 + 30.0 * s.rsnReso);
                combLen[c] = std::clamp ((int) (fs / rf), 1, (int) comb[c].size() - 1);
            }
            const float rs = std::clamp (s.resonance * 2.f, 0.f, 2.f);
            rsnFb = std::clamp (s.rsnReso * (0.6f + 0.4f * rs), 0.f, 0.95f);
            rsnK = 1.f + 3.f * s.rsnReso * rs;
            rsnMixNow = std::clamp (s.rsnMix * (1.f + 2.f * z), 0.f, 1.f);
            // soft clip (CRUSH: soft & round -> hard & square)
            const float cr = std::clamp (s.crush + vCrush[v], 0.f, 1.f);
            knee = std::clamp (1.f - cr, 0.f, 1.f) * (0.5f + s.knee);
            clipGain = s.press * std::exp2 ((cr - 0.5f) * 2.f) * (1.f + 0.5f * s.subPush);
            clipComp = 1.f / (1.f + 0.25f * s.subPush);
            // stereo (STEREO + ZAAG); the side is high-passed first, so nothing here can widen the lows
            sc1.hp (fs, s.sideCut, 0.5412); sc2.hp (fs, s.sideCut, 1.3066); sc3.hp (fs, s.sideCut, 0.5412); sc4.hp (fs, s.sideCut, 1.3066);
            sbst.bell (fs, s.sideBoostHz, s.sideBoost, 1.0);
            sideGain = s.width * 2.f * s.stereo * (1.f + z) * 1.5f;
            hvAmt = s.hverb * 2.f * s.stereo;
            hv1.hp (fs, 2500.0, 0.7071); hv2.hp (fs, 2500.0, 0.7071);
            rev.set (1.1f, 900.f, 9000.f, 15.f);
            // sub
            sb1.lp (fs, s.subFreq, 0.7071); sb2.lp (fs, s.subFreq, 0.7071); sd1.lp (fs, s.subFreq, 0.7071); sd2.lp (fs, s.subFreq, 0.7071);
            subGain = s.sub * std::exp2 (-2.f * b);
            subDuck = v == 2 ? 1.f - shelfNow : 0.f;
            outGain = 1.6f;                                                        // the zaag over the sub (the OPS balance)
            dirtAmt = s.dirt * (1.f + 1.5f * z) * (0.5f + s.drive);
        }

        double fs = 48000.0, pa = 0.0, pb = 0.0, hz = 50.0;
        int blk = 0, d3Type = 1;
        float g1 = 1.f, g2 = 1.f, g3 = 1.f, d1Asym = 0.f, charBlend = 0.5f, lowIn = 0.6f, clipGain = 1.f, clipComp = 1.f, knee = 0.5f;
        uint32_t seed = 0x1234567u;
        float dirtAmt = 0.f, sideGain = 1.f, hvAmt = 0.f, subGain = 1.f, subDuck = 0.f, outGain = 1.f, shelfNow = 0.f, sweepHzNow = 0.f;
        float rsnFb = 0.5f, rsnK = 1.f, rsnMixNow = 0.25f, apA[2] { 0.f, 0.f }, ap[2][4] {}, phFb[2] {}, hold3[2] {};
        int holdN[2] {}, combLen[2] { 100, 100 }, combIdx[2] {};
        std::vector<float> comb[2];
        bq::Biquad nz[2], lowX[2], f1[2], m1[2], lc1[2], sh2[2], pk3[2], rbp[2], push[2], lmc[2], hie[2], xo1[2], xo2[2];
        bq::Biquad sc1, sc2, sc3, sc4, sbst, hv1, hv2, sb1, sb2, sd1, sd2;
        fdn::Fdn rev;
    };
}
