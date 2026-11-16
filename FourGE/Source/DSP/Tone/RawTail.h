#pragma once
// =====================================================================
//  4GE - TAIL TYPE RAW (owner's spec 2026-11-16, FourGE/TAIL_RAW_EXP.md): aggressive / distorted, Rawstyle / Xtra Raw.
//  Three things: a strong clear MID (the growl), dense harmonics from CASCADED distortion, tonal MOVEMENT while it rings out.
//  Its own engine (rule 0: never the ZAAG algorithm), on the shared tail skeleton:
//    01 SOURCE    OSC 1 SINE / TRI / SAW / SQUARE (TRI) + OSC 2 SINE / TRI / SAW / OFF + NOISE WHITE / PINK / OFF
//    02 PITCH     a controlled drop (START st -> the note, DROP ms, CURVE) - the hit, in ms
//    04 STACK     a FIXED optimised chain, every stage switchable:
//                 PRE-EQ -> DIST 1 (aggressive: the main harmonics) -> SHAPING EQ (the GROWL bell, ~400-500 Hz to start)
//                 -> DIST 2 (strong: densifies) -> BAND-PASS SWEEP -> DIST 3 (controlled: the final character)
//                 -> POST-EQ -> SOFT CLIPPER (soft / hard). Every stage its own processor, never the same drive.
//    05 FILTER    the automatic resonant BAND-PASS SWEEP: START -> (MID) -> END, RESONANCE, SPEED, CURVE, MIX,
//                 shape DOWN / UP / DOWN-UP / UP-DOWN / CUSTOM; SYNC (a part of the TAIL length) / FREE (ms) / BODY END
//    06 CHARACTER GROWL (mid harmonics + resonance), CRUNCH (distortion + clip aggression), the shared 8 TAIL macros
//                 DRIVE / TONE / MOVEMENT / RESONANCE / DENSITY / SPACE / CHAOS / WIDTH (RAW mapping)
//    07 STEREO    a stable centre: only the highs (above ~250 Hz) get a controlled width; the lows always mono
//    08 SUB       the lows of the distorted tail (under SUB HZ) blended with a clean sine on the tail pitch, mono
//    REVERB       optional, subtle, after the attack (it never smears the hit) and low-cut (no bass tail)
//    09 OUTPUT    the tail shape over the KICK LENGTH (attack / body / decay / fade)
//  CHAOS is deterministic: the same note + preset = the same render (the seed resets per kick). DSP only, no allocation in process().
// =====================================================================
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <array>
#include "Biquad.h"
#include "Fdn.h"

namespace fge::dsp::raw
{
    struct Settings
    {
        bool on = false;
        // 01 source
        int osc1 = 1, osc2 = 3, noise = 2; float osc2Level = 0.5f, osc2St = 12.f, noiseLevel = 0.3f;
        // 02 pitch
        float startSt = 24.f, dropMs = 70.f, curve = 0.6f, rootSt = 0.f;
        // 04 stack (stage switches)
        bool preOn = true, d1On = true, shapeOn = true, d2On = true, bpOn = true, d3On = true, postOn = true, clipOn = true;
        float lowCut = 30.f, preMidDb = 3.f;
        int d1Type = 2, d2Type = 1, d3Type = 0; float d1Db = 30.f, d2Db = 14.f, d3Db = 12.f;
        float growlHz = 450.f, growlDb = 8.f, growlQ = 2.f;
        float postLowCut = 25.f, postHighDb = 0.f, clipHard = 0.3f;
        // 05 filter movement
        int fltShape = 0, sync = 0; float fltStart = 1800.f, fltEnd = 600.f, fltMid = 1200.f, fltReso = 0.5f, fltSpeed = 0.8f, fltTimeMs = 250.f, fltCurve = 0.5f, fltMix = 0.7f;
        float bodyEndMs = 120.f;                       // (SYNC TO BODY END)
        // 06 character + the shared TAIL macros (0.5 = neutral)
        float growl = 0.5f, crunch = 0.5f;
        float mDrive = 0.5f, mTone = 0.5f, mMove = 0.5f, mReso = 0.5f, mDensity = 0.5f, mSpace = 0.5f, mChaos = 0.5f, mWidth = 0.5f;
        // reverb
        float verb = 0.08f, verbSize = 0.8f, verbDecay = 600.f, verbDamp = 7000.f, verbPre = 10.f, verbLowCut = 400.f;
        // sub + stereo
        float sub = 0.8f, subHz = 110.f, width = 0.3f;
        // 09 tail shape (length = KICK LENGTH)
        float lengthMs = 400.f, attackMs = 2.f, body = 0.6f, decay = 0.5f, fade = 0.5f;
    };

    // ---- the sweep position 0..1 -> a frequency between START / MID / END (log), by the movement shape (pure: the tests call it)
    inline float sweepHz (float u, int shape, float start, float mid, float end, float curve)
    {
        u = std::clamp (u, 0.f, 1.f);
        const float k = 0.4f + 2.2f * std::clamp (curve, 0.f, 1.f);                    // CURVE: fast early (low) <-> late (high)
        auto lerpLog = [] (float a, float b, float t) { return std::exp (std::log (std::max (20.f, a)) + (std::log (std::max (20.f, b)) - std::log (std::max (20.f, a))) * t); };
        auto ease = [k] (float t) { return 1.f - std::pow (1.f - std::clamp (t, 0.f, 1.f), k); };
        switch (shape)
        {
            case 1: return lerpLog (end, start, ease (u));                                                          // UP
            case 2: return u < 0.5f ? lerpLog (start, end, ease (u * 2.f)) : lerpLog (end, start, ease (u * 2.f - 1.f));   // DOWN-UP
            case 3: return u < 0.5f ? lerpLog (end, start, ease (u * 2.f)) : lerpLog (start, end, ease (u * 2.f - 1.f));   // UP-DOWN
            case 4: return u < 0.5f ? lerpLog (start, mid, ease (u * 2.f)) : lerpLog (mid, end, ease (u * 2.f - 1.f));     // CUSTOM (start -> mid -> end)
            default: return lerpLog (start, end, ease (u));                                                        // DOWN
        }
    }

    class Unit
    {
    public:
        void prepare (double rate) { fs = rate; rev.prepare (rate, 2.0, 80.0); reset(); }
        void reset()
        {
            p1 = p2 = 0.0; blk = 0; rev.reset(); seed = 0x2b7e1516u; chaosNow = 0.f; chaosT = 0.f;
            pk0 = pk1 = pk2 = 0.f; dly.fill (0.f); di = 0;
            for (auto* b : { &pre1, &pre2, &dc1, &grl, &bp, &dc3, &post1, &post2, &lowA, &lowB, &hiA, &hiB, &sA, &sB, &vh1, &vh2, &nzl }) b->clear();
        }

        void process (const Settings& s, float ms, float noteHz, float& outL, float& outR)
        {
            if (blk-- <= 0) { blk = 32; cook (s, ms, noteHz); }
            // ---- 01 SOURCE
            const float a = wave (s.osc1, p1);
            float x = a;
            if (s.osc2 != 3) x += s.osc2Level * wave (s.osc2, p2);
            if (s.noise != 2)
            {
                float n = noise();
                if (s.noise == 1) { pk0 = 0.997f * pk0 + 0.029591f * n; pk1 = 0.985f * pk1 + 0.032534f * n; pk2 = 0.950f * pk2 + 0.048056f * n; n = 4.f * (pk0 + pk1 + pk2 + 0.0187f * n); }   // PINK
                x += s.noiseLevel * nzl.tick (n);
            }
            const float sine = (float) std::sin (6.283185307 * p1);
            p1 += hz / fs; if (p1 >= 1.0) p1 -= 1.0;
            p2 += hz * osc2Ratio / fs; if (p2 >= 1.0) p2 -= 1.0;
            // ---- 04 STACK (each stage its own processor; the drives fall stage by stage)
            if (s.preOn) x = pre2.tick (pre1.tick (x));                                         // PRE-EQ: low cut + mid
            if (s.d1On) x = dc1.tick (stage (s.d1Type, x * g1));                                // DIST 1: aggressive, the main harmonics
            if (s.shapeOn) x = grl.tick (x);                                                    // SHAPING EQ: the GROWL bell
            if (s.d2On) x = stage (s.d2Type, x * g2);                                           // DIST 2: densifies
            if (s.bpOn) x += fltMixNow * bpGain * bp.tick (x);                                  // BAND-PASS SWEEP as an EMPHASIS on the full stack (owner 2026-11-16: replacing
                                                                                                //  the signal with a narrow resonant band-pass = an ACID squelch, not a raw growl)
            if (s.d3On) x = dc3.tick (stage (s.d3Type, x * g3) * d3Comp);                       // DIST 3: controlled, the final character
            if (s.postOn) x = post2.tick (post1.tick (x));                                      // POST-EQ: low cut + TONE shelf
            if (s.clipOn) x = clip (x * clipIn);                                                // SOFT CLIPPER (soft <-> hard)
            // ---- 08 SUB MANAGEMENT: the lows (under SUB HZ) = the distorted lows blended with a clean sine on the pitch, mono
            const float low = lowB.tick (lowA.tick (x)), high = hiB.tick (hiA.tick (x));
            const float lows = low + (subSine * sine - low) * subBlend;
            // ---- 07 STEREO: a stable centre; a controlled width only above ~250 Hz (a short-delay difference of the highs)
            dly[(size_t) di] = high; const float hd = dly[(size_t) ((di + dlyN) % (int) dly.size())]; di = (di + 1) % (int) dly.size();
            float side = sB.tick (sA.tick (high - hd)) * sideGain;
            float mid = lows + high;
            // ---- REVERB: after the attack, low-cut, subtle
            if (verbNow > 0.f)
            {
                float rl = 0.f, rr = 0.f;
                rev.tick (vh2.tick (vh1.tick (high)) * verbSend, rl, rr);
                mid += verbNow * 0.5f * (rl + rr); side += verbNow * 0.8f * (rl - rr);
            }
            // ---- 09 OUTPUT: the tail shape
            const float e = shapeEnv (s, ms);
            outL = (mid + side) * e * 1.4f;                                                     // (level-matched to the classic tail; the MASTER CLIP takes it to 0 dB)
            outR = (mid - side) * e * 1.4f;
        }

        // (tests)
        float lastSweepHz() const { return sweepNow; }
        float lastPitchHz() const { return (float) hz; }

    private:
        static float wave (int type, double p)
        {
            switch (type)
            {
                case 0: return (float) std::sin (6.283185307 * p);
                case 2: return (float) (2.0 * p - 1.0);
                case 3: return p < 0.5 ? 0.8f : -0.8f;
                default: return (float) (4.0 * std::abs (p - 0.5) - 1.0);
            }
        }
        // the stage curves: SOFT / HARD / ASYMMETRIC / WAVE / FOLD / TUBE (drive already applied)
        static float stage (int type, float x)
        {
            switch (type)
            {
                case 1: return std::clamp (x, -1.f, 1.f);
                case 2: return x >= 0.f ? std::tanh (1.5f * x) : 0.6f * std::tanh (0.8f * x);
                case 3: return std::sin (1.5707963f * std::tanh (1.3f * x));
                case 4: return std::sin (1.5707963f * std::clamp (x, -3.f, 3.f));
                case 5: return x >= 0.f ? 1.f - std::exp (-x) : -(1.f - std::exp (0.8f * x)) * 0.9f;
                default: return std::tanh (x);
            }
        }
        float clip (float x) const
        {
            const float t = std::tanh (x), h = std::clamp (x, -1.f, 1.f);
            return 0.9f * (t + (h - t) * clipHardNow);
        }
        float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f); }
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
            const float L = std::max (20.f, s.lengthMs);
            // CHAOS: a slow, deterministic random walk (the distortion's instability)
            chaosT += 32.f / (float) fs * 1000.f;
            if (chaosT >= 12.f) { chaosT = 0.f; chaosNow = 0.6f * chaosNow + 0.4f * noise(); }
            const float chaos = std::clamp ((s.mChaos - 0.5f) * 2.f, 0.f, 1.f) * chaosNow;
            // 02 PITCH: a controlled drop (ms)
            const float u = std::clamp (ms / std::max (1.f, s.dropMs), 0.f, 1.f);
            const float fall = std::pow (1.f - u, 1.f + 3.f * s.curve);
            const double root = std::max (10.0, (double) noteHz * std::exp2 (s.rootSt / 12.0));
            hz = std::clamp (root * std::exp2 (s.startSt * fall / 12.0), 5.0, fs * 0.3);
            osc2Ratio = std::exp2 (s.osc2St / 12.0);
            // the macros (RAW mapping)
            const float dr = s.mDrive - 0.5f, gr = s.growl - 0.5f, cr = s.crunch - 0.5f, dn = s.mDensity - 0.5f, rs = s.mReso - 0.5f, tn = s.mTone - 0.5f;
            const float drvDb = dr * 18.f + cr * 10.f + chaos * 6.f;                            // DRIVE + CRUNCH + CHAOS
            g1 = std::pow (10.f, (s.d1Db + drvDb) / 20.f);
            g2 = std::pow (10.f, (s.d2Db + 0.6f * drvDb + dn * 14.f) / 20.f);                    // DENSITY: the densifying stage
            g3 = std::pow (10.f, (s.d3Db + 0.3f * drvDb) / 20.f);
            d3Comp = 1.f;
            clipHardNow = std::clamp (s.clipHard + cr * 1.2f, 0.f, 1.f);                        // CRUNCH: harder clipping
            clipIn = std::pow (10.f, (2.f + cr * 6.f) / 20.f);
            // 05 FILTER MOVEMENT (SYNC = a part of the TAIL length / FREE = ms / BODY END)
            float w;
            if (s.sync == 1) w = ms / std::max (5.f, s.fltTimeMs);
            else if (s.sync == 2) w = (ms - s.bodyEndMs) / std::max (5.f, (L - s.bodyEndMs) * std::max (0.05f, s.fltSpeed));
            else w = ms / std::max (5.f, L * std::max (0.05f, s.fltSpeed));
            const float f = sweepHz (w, s.fltShape, s.fltStart, s.fltMid, s.fltEnd, s.fltCurve);
            const float mv = std::clamp (s.mMove * 2.f, 0.f, 2.f);                               // MOVEMENT: the depth of the sweep (0 = parked in the middle)
            const float centre = std::sqrt (std::max (20.f, s.fltStart) * std::max (20.f, s.fltEnd));
            sweepNow = std::clamp (std::exp (std::log (centre) + (std::log (std::max (20.f, f)) - std::log (centre)) * mv), 60.f, (float) fs * 0.4f);
            const double q = std::clamp (0.6 + 3.4 * (double) s.fltReso * (1.0 + rs + 0.6 * gr), 0.5, 8.0);    // RESONANCE + GROWL (wide: a moving formant, never a 303 peak)
            bp.bp (fs, sweepNow, q);
            bpGain = 1.2f + 0.15f * (float) q;
            fltMixNow = std::clamp (s.fltMix, 0.f, 1.f);
            // EQs
            pre1.hp (fs, std::max (10.f, s.lowCut), 0.7071);
            pre2.bell (fs, 700.0, s.preMidDb, 0.8);
            const float grlHz = s.growlHz * std::exp2 (tn * 0.8f);                               // TONE moves the growl up / down
            grl.bell (fs, std::clamp ((double) grlHz, 80.0, fs * 0.4), s.growlDb + gr * 14.f, std::clamp ((double) s.growlQ * (1.0 + 0.8 * gr), 0.3, 12.0));   // GROWL
            post1.hp (fs, std::max (10.f, s.postLowCut), 0.7071);
            post2.shelf (fs, 3000.0, s.postHighDb + tn * 10.f, true);                           // TONE: brightness
            dc1.hp (fs, 15.0, 0.7071); dc3.hp (fs, 15.0, 0.7071);
            nzl.lp (fs, 9000.0, 0.7071);
            // SUB + STEREO
            lowA.lp (fs, s.subHz, 0.7071); lowB.lp (fs, s.subHz, 0.7071);
            hiA.hp (fs, s.subHz, 0.7071); hiB.hp (fs, s.subHz, 0.7071);
            subBlend = std::clamp (s.sub, 0.f, 1.f); subSine = 0.8f * std::max (1.f, s.sub);
            sA.hp (fs, 250.0, 0.7071); sB.hp (fs, 250.0, 0.7071);                                // the side only above ~250 Hz
            sideGain = std::clamp (s.width * 2.f * s.mWidth, 0.f, 1.5f);
            dlyN = std::clamp ((int) (fs * 0.0004), 1, (int) dly.size() - 1);
            // REVERB: subtle, after the attack (a 25 ms fade-in of the send), the lows cut (no bass tail)
            verbNow = std::clamp (s.verb * 2.f * s.mSpace, 0.f, 1.f);
            verbSend = std::clamp ((ms - 5.f) / 25.f, 0.f, 1.f);
            vh1.hp (fs, s.verbLowCut, 0.7071); vh2.hp (fs, s.verbLowCut, 0.7071);
            rev.set (std::clamp (s.verbSize, 0.2f, 2.f), s.verbDecay, s.verbDamp, s.verbPre);
        }

        double fs = 48000.0, p1 = 0.0, p2 = 0.0, hz = 50.0, osc2Ratio = 2.0;
        int blk = 0, di = 0, dlyN = 20;
        uint32_t seed = 0x2b7e1516u;
        float g1 = 1.f, g2 = 1.f, g3 = 1.f, d3Comp = 1.f, clipHardNow = 0.3f, clipIn = 1.f, bpGain = 1.f, fltMixNow = 0.6f, sweepNow = 1000.f;
        float subBlend = 0.8f, subSine = 0.8f, sideGain = 0.3f, verbNow = 0.f, verbSend = 0.f, chaosNow = 0.f, chaosT = 0.f, pk0 = 0.f, pk1 = 0.f, pk2 = 0.f;
        std::array<float, 256> dly {};
        bq::Biquad pre1, pre2, dc1, grl, bp, dc3, post1, post2, lowA, lowB, hiA, hiB, sA, sB, vh1, vh2, nzl;
        fdn::Fdn rev;
    };
}
