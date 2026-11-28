#pragma once
// =====================================================================
//  4GE - TAIL TYPE HARDCORE (owner spec 2026-11-24 "HARDCORE KICK ENGINE", FourGE/HARDCORE_ENGINE.md): ONE engine for
//  GABBER / MILLENNIUM / HARDCORE / UPTEMPO - the styles are PROFILES (StyleEngine values), never separate generators.
//  In hardcore the kick is the whole distorted signal (gabber: KICK = BASSLINE), so this engine makes the whole kick (like ZAAG):
//    SOURCE (909 / 909 HARD / BD ANALOG / SINE / SINE + 909 / HYBRID, SOURCE MIX)
//    -> PITCH (END = the note / a FIXED KEY + END st + FINE, START = END + DROP st, TIME, CURVE LINEAR / EXP / FAST DROP / S-CURVE / CUSTOM)
//    -> PUNCH / BODY / TAIL relation (time segments: their own drive into the chain, the tail fades over the kick length)
//    -> PRE-EQ (LOW / LOW-MID / MID / HIGH-MID / HIGH: gain + freq) -> DRIVE 1 (tube / soft: the first harmonics) -> EQ (between)
//    -> DRIVE 2 (hard: the main character) [<- the tonal CRUNCH layer joins here, clipped together with the kick]
//    -> RESAMPLE FEEL (internal normalisation + a second pass, like a re-rendered kick) -> ASYMMETRY (+ drive / - drive / bias)
//    -> CLIP (HARD / SOFT / FOLD / ASYMMETRIC / CUSTOM) -> POST-EQ (LOW CUT, BODY, MID, CRUNCH band 2-6 kHz, HIGH)
//  STAGES 1 / 2 / 3: 1 = DRIVE 1 -> CLIP, 2 = + DRIVE 2, 3 = + ASYMMETRY. TEXTURE (noise / crushed noise / a short transient) is only
//  an optional, limited addition - never the crunch. Deterministic: the same note + preset = the same render. DSP only, no allocation.
// =====================================================================
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "Biquad.h"
#include "../Dist/FastDist.h"

namespace fge::dsp::hardcore
{
    struct Settings
    {
        bool on = false;
        // SOURCE
        int source = 0; float srcMix = 0.5f;                       // 909 / 909 HARD / BD ANALOG / SINE / SINE + 909 / HYBRID
        // PITCH
        float dropSt = 30.f, timeMs = 25.f, customCurve = 0.5f, endSt = 0.f, fineCt = 0.f, fixedNote = 31.f;
        int curve = 1, keyMode = 0;                                // LINEAR / EXP / FAST DROP / S-CURVE / CUSTOM; FOLLOW MIDI / FIXED KEY
        // PUNCH / BODY / TAIL relation
        float punchMs = 30.f, bodyMs = 90.f, punchDb = 3.f, bodyDb = 0.f, tailDb = -3.f, tailDecay = 0.6f, attackMs = 0.3f;
        // CRUNCH (tonal)
        bool crOn = true; int crSrc = 0, crPitchMode = 0;          // 909 / BD, SINE, SINE + 909, SAW; OWN / FOLLOW PUNCH / FOLLOW BODY
        float crStartMs = 10.f, crLenMs = 80.f, crPitchSt = 12.f, crPitchAmt = 7.f, crPitchMs = 30.f, crMix = 0.4f, crDrive = 0.5f, crAsym = 0.5f;
        // TEXTURE (optional, limited)
        int txType = 0; float txMix = 0.f, txLenMs = 15.f;         // NOISE / CRUSHED NOISE / SHORT TRANSIENT
        // DISTORTION
        float eqLowDb = 0.f, eqLowHz = 70.f, eqLmDb = 0.f, eqLmHz = 250.f, eqMidDb = 0.f, eqMidHz = 900.f, eqHmDb = 0.f, eqHmHz = 2500.f, eqHighDb = 0.f, eqHighHz = 8000.f;
        float d1Db = 12.f, betweenHz = 600.f, betweenDb = 0.f, d2Db = 18.f, asym = 0.5f, bias = 0.2f, clipDb = 6.f, resample = 0.f;
        float asymPosDb = 0.f, asymNegDb = 0.f;
        float holdMs = 0.f, dropXSt = 0.f;                         // PITCH HOLD (the START held, a tok) + DROP + (st beyond DROP: START = END + DROP + DROP +)
        float msDip = 0.f, msDipAtMs = 85.f, msDipLenMs = 120.f;
        float msClick = 0.f;
        bool msOwn = false;
        float clip2Db = 0.f, lpHz = 20000.f;
        float fuzzBias = 0.f;
        float pnMix = 0.f, pnHoldMs = 10.f, pnLenMs = 40.f, pnHz = 2500.f, pnGrain = 0.f;   // PRE NOISE (owner 2026-11-28 "punch musi byc gloska CZ"): band noise INTO the drives
        float harmDb = 0.f; int harmN = 1;                         // MILLENNIUM spec 11: HARMONIC EMPHASIS before the distortion (a bell at 1x / 2x / 3x / 4x the note, follows the note)
        float compAmt = 0.f, compAtkMs = 50.f, compRelMs = 120.f;
        float clickLenMs = 0.12f, clickNoise = 0.f;
        float clickBal = 0.f;
        float preDecay = 0.f;
        fastdist::Settings fd1, fd2;
        float outPunchDb = 0.f, outBodyDb = 0.f;                   // OUT PUNCH / OUT BODY: the part levels AFTER the distortion (a drive cannot pull a quiet start back up)
        bool msPost = false;                                       // MID SCREAM AFTER THE DRIVE: clicks / bursts join after the clip - they stay sharp                               // FAST DIST x 2 right after the PRE-EQ (owner spec FAST DIST STAGE; the gabber tutorial: 2x Fast Dist, PRE 100 %)                                      // PRE DECAY: the level falls BEFORE the distortion over the tail (909 -> pedal: the end gets rounder, the tail moves)                                      // CLICK BALANCE: 0 = the 2 clicks per cycle equal and opposite (odd harmonics only),
                                                                   //  0.5 = the 2nd click silent, 1 = both the same way (the ref's even = odd harmonics)                // CLICK LENGTH + CLICK NOISE: each click a short (seeded) noise burst - the gabber "brrr" (2 bursts per cycle in the owner's refs)  // MILLENNIUM spec 23-25: a COMPRESSOR between DRIVE 1 and DRIVE 2 (attack ~50 ms keeps the punch)                                      // FUZZ BIAS: a DC offset BEFORE DRIVE 1 - the square's duty cycle moves (60 / 40): even harmonics, the "farty" fuzz                       // GABBER spec (owner 2026-11-24): HARD CLIP 2 after the POST-EQ (0 = off) + the DISTORTION LOW-PASS (20 kHz = off)                                        // MID SCREAM OWN LEVEL: not shaped by the PUNCH / BODY / TAIL gains (a buzz over a silent punch)                                       // MID SCREAM CLICK: the wave -> a short impulse at every cycle start, alternating up / down (gabber 909 "tok-tok")   // MID SCREAM DIP: the 2nd tone pulled down (x 1 - DIP) from DIP AT for DIP LEN, then back (the clean "DUM")                    // ASYMMETRY + DRIVE / - DRIVE (spec pt 19: each half its own drive, 0 dB = the ASYMMETRY knob alone)
        int stages = 3, clipMode = 0;                              // HARD / SOFT / FOLD / ASYMMETRIC / CUSTOM
        // POST-EQ
        float postLowCut = 30.f, postBodyDb = 0.f, postMidDb = 0.f, postCrunchDb = 0.f, postCrunchHz = 3500.f, postHighDb = 0.f;
        // LENGTH (the engine's own cap: BPM GRID) + BURST
        float lengthMs = 400.f;                                    // (the engine computes it: KICK LENGTH or the BPM grid cap)
        // MID SCREAM (owner refs 2026-11-24: a 2nd, higher tone through the WHOLE kick, clipped TOGETHER with the note's square -
        //  the notches / spikes inside every cycle = the crunchy hardcore tail; without it the tail is a plain "bzzz" pulse)
        float msMix = 0.f, msHz = 800.f, msHarm = 16.f, msSweepSt = 12.f, msSweepMs = 60.f, msStartMs = 0.f, msFadeMs = 10.f, msMotion = 0.f, msRate = 5.f;
        int msTrack = 0, msWave = 0;                               // FIXED HZ / HARMONIC OF THE NOTE / FOLLOW PITCH (x the falling pitch, phase-locked); SINE / TRIANGLE / SAW / SQUARE
        float outClipDb = 0.f;                                     // OUTPUT CLIP: the last stage (after the post EQ: flat tops, no tilt)
    };

    // the pitch fall 0..1 (1 = START, 0 = END) at u = t / TIME (pure: the tests call it)
    inline float fallOf (int curve, float u, float custom)
    {
        u = std::clamp (u, 0.f, 1.f);
        switch (curve)
        {
            case 0: return 1.f - u;                                                              // LINEAR
            case 2: return std::pow (1.f - u, 4.f);                                              // FAST DROP
            case 3: return 0.5f * (1.f + std::cos (3.14159265f * u));                            // S-CURVE
            case 4: return std::pow (1.f - u, 0.3f + 4.f * std::clamp (custom, 0.f, 1.f));       // CUSTOM
            default: { const float e4 = std::exp (-4.f); return (std::exp (-4.f * u) - e4) / (1.f - e4); }   // EXP
        }
    }

    class Unit
    {
    public:
        void prepare (double rate) { fs = rate; fdA.prepare (rate); fdB.prepare (rate); clickDecay = (float) std::exp (-1.0 / (rate * 0.00012)); reset(); }
        void reset()
        {
            p = pc = pm = 0.0; blk = 0; seed = 0x5eedbeefu; rms = 0.05f; crush = 0.f; crushN = 0;
            for (auto* b : { &eLow, &eLm, &eMid, &eHm, &eHigh, &dc1, &betw, &pLow, &pBody, &pMid, &pCr, &pHigh, &crHp, &crLp, &txBp, &dcOut, &lp1, &lp2, &eHarm, &pnBp }) b->clear(); pnN = 0; pnHeld = 0.f; cEnv = 0.f; fdA.reset(); fdB.reset();
        }

        float process (const Settings& s, float ms, float noteHz)
        {
            if (ms >= s.lengthMs) return 0.f;
            if (blk-- <= 0) { blk = 32; cook (s, ms, noteHz); }
            // ---- SOURCE
            const double ph = p; p += hz / fs; if (p >= 1.0) p -= 1.0;
            float x = source (s, ph, ms);
            // ---- PUNCH / BODY / TAIL: each segment drives the chain its own amount (the relation, not just a volume)
            const float atk = s.attackMs > 0.01f ? std::min (1.f, ms / s.attackMs) : 1.f;
            x *= atk * segGain * preEnv;
            fuzzEnv = atk * segGain * preEnv;                                    // (the bias follows the level: silence stays silence)
            // ---- MID SCREAM joins BEFORE the pre EQ / drive: the two tones are distorted together
            const float msNow = s.msMix > 0.f ? midScream (s, ms) * atk * (s.msOwn ? 1.f : segGain) * preEnv : 0.f;
            if (! s.msPost) x += msNow;   // (MIX = its level against the note: 0.3 = spikes near the note's edges)
            // ---- PRE NOISE (the "CZ"): band-passed (optionally sample-and-held = grain) noise, held then decayed, distorted WITH the tone
            if (s.pnMix > 0.f)
            {
                const float t = ms - s.pnHoldMs, e = t <= 0.f ? 1.f : std::exp (-t * 2.3f / std::max (1.f, s.pnLenMs));
                if (e > 1.0e-4f)
                {
                    if (pnN-- <= 0) { pnN = (int) (s.pnGrain * s.pnGrain * fs / 1500.0); pnHeld = noise(); }
                    x += pnBp.tick (s.pnGrain > 0.f ? pnHeld : noise()) * 2.f * s.pnMix * e * atk * preEnv;
                }
            }
            // ---- PRE-EQ (before the distortion: which band gets turned into harmonics)
            x = eHigh.tick (eHm.tick (eMid.tick (eLm.tick (eLow.tick (x)))));
            if (s.harmDb != 0.f) x = eHarm.tick (x);
            // ---- FAST DIST 1 + 2 (PRE -> THRES core A / B -> MIX -> POST): before DRIVE 1
            if (s.fd1.on) x = fdA.process (s.fd1, x);
            if (s.fd2.on) x = fdB.process (s.fd2, x);
            // ---- DRIVE 1 (tube / soft: the first harmonics)
            x = dc1.tick (std::tanh (g1 * (x + 0.15f * x * x + s.fuzzBias * fuzzEnv)) * 0.95f);
            // ---- EQ between the stages
            x = betw.tick (x);
            // ---- COMPRESSOR between the stages (shapes the punch / body relation before DRIVE 2, not loudness)
            if (s.compAmt > 0.f)
            {
                const float a = std::abs (x);
                cEnv += (a > cEnv ? cAtk : cRel) * (a - cEnv);
                const float thr = 1.f - 0.8f * std::clamp (s.compAmt, 0.f, 1.f);
                if (cEnv > thr) x *= (thr + (cEnv - thr) * 0.25f) / cEnv;
                x *= cMake;
            }
            // ---- the tonal CRUNCH joins here (it is clipped TOGETHER with the kick: several tones in one clip = the crack, not a "pup")
            crNow = crunch (s, ms); x += crNow;
            // ---- DRIVE 2 (hard: the main character)
            if (s.stages >= 2) x = std::clamp (g2 * x, -1.f, 1.f) * 0.8f + 0.2f * std::tanh (g2 * x);
            // ---- RESAMPLE FEEL: an internal normalisation (as if rendered and loaded again) + a second hard pass
            if (s.resample > 0.f)
            {
                rms += (std::abs (x) - rms) * 0.002f;
                const float n = x / std::max (0.05f, rms) * 0.35f;
                const float r2 = std::clamp (n * (1.f + 4.f * s.resample), -1.f, 1.f);
                x += (r2 - x) * s.resample;
            }
            // ---- ASYMMETRY: positive drive != negative drive + a bias (DC offset), the DC is taken out after
            if (s.stages >= 3 && (s.asym > 0.f || s.asymPosDb != 0.f || s.asymNegDb != 0.f))
            {
                const float a = s.asym, v = x + 0.4f * s.bias * std::max (a, 0.25f * (std::abs (s.asymPosDb - s.asymNegDb) / 12.f));
                x = v >= 0.f ? std::tanh ((1.f + 3.f * a) * gPos * v) : std::tanh ((1.f + 0.8f * a) * gNeg * v) * (1.f - 0.35f * a);
                x += 0.25f * a * s.bias;        // the offset stays INTO the clip: it moves the square's duty cycle (even harmonics) - the DC goes after the clip
            }
            // ---- CLIP (the asymmetry survives it: the negative half stays lower after the clip - unequal halves = even harmonics,
            //      otherwise a hard clip after a driven chain makes every asymmetry symmetric again)
            x = clip (s.clipMode, x * gc);
            if (s.stages >= 3 && x < 0.f) x *= 1.f - 0.4f * s.asym;
            if (s.msPost) x += msNow;                                          // MID SCREAM AFTER THE DRIVE (after the clip: sharp)
            // ---- TEXTURE (optional, limited: max 30 %)
            if (s.txMix > 0.f && ms < s.txLenMs * 3.f) x += texture (s, ms) * std::min (0.3f, s.txMix);
            // ---- POST-EQ (+ the DC of the asymmetry taken out after the clip)
            x = dcOut.tick (x);
            x = pHigh.tick (pCr.tick (pMid.tick (pBody.tick (pLow.tick (x)))));
            // ---- HARD CLIP 2 (GABBER spec 16: after the POST-EQ, a further controlled densification) + the DISTORTION LOW-PASS (spec 18: the fizz)
            if (s.clip2Db > 0.f) x = std::clamp (x * gClip2, -1.f, 1.f) * 0.9f;
            if (s.lpHz < 19999.f) x = lp2.tick (lp1.tick (x));
            // ---- OUTPUT CLIP = FINAL CLIP: the last stage (the post EQ's tilt clipped flat again, the hardcore "brick")
            if (s.outClipDb > 0.f) x = std::clamp (x * oc, -0.95f, 0.95f);
            // ---- the tail fades over the kick length (+ a short end fade, never a click)
            const float left = s.lengthMs - ms;
            return x * outEnv * segOut * (left < 4.f ? left / 4.f : 1.f) * 0.85f;
        }

        // (tests / the analysis readout)
        float lastPitchHz() const { return (float) hz; }

    private:
        float source (const Settings& s, double ph, float ms)
        {
            const float sn = (float) std::sin (6.283185307 * ph);
            const float tri = (float) (1.0 - 4.0 * std::abs (ph - 0.5));
            auto k909 = [&] (bool hard)
            {
                // the 909 bass drum: a triangle rounded towards a sine (its waveshaper) + the attack click (a noise tick + a short blip)
                const float body = 0.55f * sn + 0.45f * std::tanh (1.6f * tri) / std::tanh (1.6f);
                const float clk = (hard ? 1.6f : 1.f) * std::exp (-ms / (hard ? 0.6f : 1.2f)) * (0.6f * noise() + 0.4f * (float) std::sin (6.283185307 * 1100.0 * ms * 0.001));
                const float b = hard ? std::tanh (2.4f * body) / std::tanh (2.4f) : body;   // 909 HARD: a fuller (driven) body
                return b + clk;
            };
            const float bd = sn + 0.1f * (float) std::sin (2.0 * 6.283185307 * ph);           // BD ANALOG: a tonal body, a touch of the 2nd
            switch (s.source)
            {
                case 1:  return k909 (true);
                case 2:  return bd;
                case 3:  return sn;
                case 4:  return (1.f - s.srcMix) * sn + s.srcMix * k909 (false);               // SINE + 909 (the sine = the fundamental, 909 = the character)
                case 5:  return (1.f - s.srcMix) * k909 (true) + s.srcMix * bd + 0.15f * (ph < 0.5 ? 1.f : -1.f);   // HYBRID (+ a little square bite)
                default: return k909 (false);
            }
        }
        float crunch (const Settings& s, float ms)
        {
            if (! s.crOn || s.crMix <= 0.f) return 0.f;
            const float t = ms - s.crStartMs;
            if (t < 0.f || t > s.crLenMs + 8.f) { if (t < 0.f) pc = 0.0; return 0.f; }
            double f;
            switch (s.crPitchMode)
            {
                case 1:  f = hz * std::exp2 (s.crPitchSt / 12.0); break;                                                   // FOLLOW PUNCH (its drop)
                case 2:  f = endHz * std::exp2 (s.crPitchSt / 12.0); break;                                                // FOLLOW BODY (steady)
                default: f = endHz * std::exp2 ((s.crPitchSt + s.crPitchAmt * std::exp (-t / std::max (1.f, s.crPitchMs))) / 12.0); break;   // OWN
            }
            f = std::clamp (f, 20.0, fs * 0.2);
            const double ph = pc; pc += f / fs; if (pc >= 1.0) pc -= 1.0;
            const float sn = (float) std::sin (6.283185307 * ph), tri = (float) (1.0 - 4.0 * std::abs (ph - 0.5));
            float x;
            switch (s.crSrc)
            {
                case 1:  x = sn; break;                                                                                     // SINE
                case 2:  x = 0.5f * sn + 0.5f * (0.55f * sn + 0.45f * tri); break;                                          // SINE + 909
                case 3:  x = (float) (2.0 * ph - 1.0); break;                                                               // SAW
                default: x = 0.55f * sn + 0.45f * std::tanh (1.6f * tri) / std::tanh (1.6f); break;                       // 909 / BD
            }
            const float env = std::min (1.f, t / 0.5f) * (t < s.crLenMs ? 1.f : std::max (0.f, 1.f - (t - s.crLenMs) / 8.f));
            x = crLp.tick (crHp.tick (x * env));                                                                           // (no sub from it)
            x = std::tanh ((1.f + 6.f * s.crDrive) * x);                                                                   // SATURATION
            const float a = s.crAsym;                                                                                      // ASYMMETRIC
            x = x >= 0.f ? std::tanh ((1.f + 4.f * a) * x) : std::tanh ((1.f + a) * x) * (1.f - 0.4f * a);
            return std::clamp (1.5f * x, -1.f, 1.f) * s.crMix;                                                             // CLIP -> MIX
        }
        float midScream (const Settings& s, float ms)
        {
            const float t = ms - s.msStartMs;
            if (t < 0.f) { pm = 0.0; clickEnv = 0.f; clickSign = 1.f; return 0.f; }
            const double base = s.msTrack == 1 ? endHz * (double) s.msHarm : s.msTrack == 2 ? hz * (double) s.msHarm : (double) s.msHz;
            const double sweep = s.msSweepSt * std::exp (-t / std::max (1.f, s.msSweepMs));                 // starts SWEEP st away, glides home
            const double vib = s.msMotion * std::sin (6.283185307 * s.msRate * ms * 0.001);                   // MOTION: +-1 st movement
            const double f = std::clamp (base * std::exp2 ((sweep + vib) / 12.0), 40.0, fs * 0.2);
            double ph = pm;
            if (s.msTrack == 2)                                                                 // FOLLOW PITCH: locked to the note's phase (cycle starts at
            {                                                                                   //  the note's peaks for an even harmonic: the 909 "tok" spikes)
                const double h = std::max (1.0, std::round ((double) s.msHarm));
                const double np = h * (p + 0.95); ph = np - std::floor (np);          // (0.95: lands on the peaks after the chain's phase shift)
                if (ph < pm - 0.5) { clickSign = ((int64_t) std::floor (np) & 1) == 1 ? 1.f : -1.f;      // (by the note's phase: the same click is
                                     clickEnv = clickSign > 0.f ? 1.f : -(1.f - 2.f * clickBalNow); }       //  always the "up" one, whenever it starts)
                pm = ph;
            }
            else
            {
                pm += f / fs;
                if (pm >= 1.0) { pm -= 1.0; clickSign = -clickSign; clickEnv = clickSign > 0.f ? 1.f : -(1.f - 2.f * clickBalNow); }   // a new cycle: the CLICK fires
            }
            float x;
            switch (s.msWave)
            {
                case 1:  x = (float) (1.0 - 4.0 * std::abs (ph - 0.5)); break;
                case 2:  x = (float) (2.0 * ph - 1.0); break;
                case 3:  x = ph < 0.5 ? 0.8f : -0.8f; break;
                default: x = (float) std::sin (6.283185307 * ph); break;
            }
            if (s.msClick > 0.f)
            {
                float c = clickEnv; clickEnv *= clickDecay;                                                  // CLICK LENGTH (0.12 ms = an impulse)
                if (s.clickNoise > 0.f) c *= 1.f - s.clickNoise + s.clickNoise * 2.2f * noise();          // CLICK NOISE: the burst turns into grit
                x += (c - x) * std::clamp (s.msClick, 0.f, 1.f);
            }
            const float fade = s.msFadeMs > 0.5f ? std::min (1.f, t / s.msFadeMs) : 1.f;
            return x * fade * s.msMix * dipGain (s, ms);
        }
        // the DIP: in over 20 ms around DIP AT, held for DIP LEN, back over 50 ms (owner's OPS hardcore refs: the 2nd tone almost gone at
        // ~85-150 ms - a clean note square - then back to the end)
        static float dipGain (const Settings& s, float ms)
        {
            if (s.msDip <= 0.f) return 1.f;
            const float in = std::clamp ((ms - (s.msDipAtMs - 10.f)) / 20.f, 0.f, 1.f);
            const float out = std::clamp ((ms - (s.msDipAtMs + s.msDipLenMs)) / 50.f, 0.f, 1.f);
            return 1.f - std::clamp (s.msDip, 0.f, 1.f) * in * (1.f - out);
        }
        float texture (const Settings& s, float ms)
        {
            const float e = std::exp (-ms / std::max (0.5f, s.txLenMs * 0.4f));
            switch (s.txType)
            {
                case 1:                                                                                                     // CRUSHED NOISE (sample-and-hold)
                    if (crushN-- <= 0) { crushN = (int) (fs / 6000.0); crush = noise(); }
                    return std::round (crush * 4.f) / 4.f * e;
                case 2:  return txBp.tick (noise()) * 3.f * std::exp (-ms / 1.5f);                                         // SHORT TRANSIENT
                default: return noise() * e;                                                                               // NOISE
            }
        }
        static float clip (int mode, float x)
        {
            switch (mode)
            {
                case 1:  return std::tanh (x);                                                                              // SOFT
                case 2:  return std::sin (1.5707963f * std::clamp (x, -3.f, 3.f));                                          // FOLD
                case 3:  return x >= 0.f ? std::min (1.f, x) : std::max (-0.75f, std::tanh (x));                            // ASYMMETRIC
                case 4:  { const float c = std::clamp (x, -1.5f, 1.5f); return c - c * c * c / 6.75f; }                    // CUSTOM (a cubic knee, 0 dB peak)
                default: return std::clamp (x, -1.f, 1.f);                                                                  // HARD
            }
        }
        float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f); }
        static float dbg (float db) { return std::pow (10.f, db / 20.f); }
        void cook (const Settings& s, float ms, float noteHz)
        {
            // PITCH: END = the note (FOLLOW MIDI) or the FIXED KEY, + END st + FINE; START = END + DROP st
            const double key = s.keyMode == 1 ? 440.0 * std::exp2 ((s.fixedNote - 69.0) / 12.0) : std::max (10.0, (double) noteHz);
            endHz = std::clamp (key * std::exp2 ((s.endSt + 0.01 * s.fineCt) / 12.0), 15.0, 2000.0);
            const float fall = fallOf (s.curve, std::max (0.f, ms - s.holdMs) / std::max (1.f, s.timeMs), s.customCurve);
            hz = std::clamp (endHz * std::exp2 ((s.dropSt + s.dropXSt) * fall / 12.0), 15.0, fs * 0.3);
            // PUNCH / BODY / TAIL segments (smooth hand-overs) + the output fade over the tail
            const float pEnd = s.punchMs, bEnd = s.punchMs + s.bodyMs;
            const float wP = std::clamp ((pEnd + 3.f - ms) / 6.f, 0.f, 1.f), wB = std::clamp ((bEnd + 5.f - ms) / 10.f, 0.f, 1.f) - wP;
            const float wT = 1.f - wP - std::max (0.f, wB);
            segGain = wP * dbg (s.punchDb) + std::max (0.f, wB) * dbg (s.bodyDb) + std::max (0.f, wT) * dbg (s.tailDb);
            segOut = wP * dbg (s.outPunchDb) + std::max (0.f, wB) * dbg (s.outBodyDb) + std::max (0.f, wT);
            const float u = std::clamp ((ms - bEnd) / std::max (10.f, s.lengthMs - bEnd), 0.f, 1.f);
            outEnv = ms < bEnd ? 1.f : std::max (0.f, 1.f - s.tailDecay * std::pow (u, 0.7f));
            preEnv = ms < bEnd ? 1.f : std::max (0.02f, 1.f - std::clamp (s.preDecay, 0.f, 1.f) * std::pow (u, 0.8f));
            // the stage gains + the EQs
            g1 = dbg (s.d1Db); g2 = dbg (s.d2Db); gc = dbg (s.clipDb); oc = dbg (s.outClipDb); gClip2 = dbg (s.clip2Db); gPos = dbg (s.asymPosDb); gNeg = dbg (s.asymNegDb);
            eLow.shelf (fs, s.eqLowHz, s.eqLowDb, false); eLm.bell (fs, s.eqLmHz, s.eqLmDb, 0.9); eMid.bell (fs, s.eqMidHz, s.eqMidDb, 0.9);
            eHm.bell (fs, s.eqHmHz, s.eqHmDb, 0.9); eHigh.shelf (fs, std::min ((double) s.eqHighHz, fs * 0.4), s.eqHighDb, true);
            dc1.hp (fs, 12.0, 0.7071); dcOut.hp (fs, 10.0, 0.7071);
            betw.bell (fs, s.betweenHz, s.betweenDb, 0.8);
            pLow.hp (fs, std::max (10.f, s.postLowCut), 0.7071); pBody.shelf (fs, 150.0, s.postBodyDb, false); pMid.bell (fs, 800.0, s.postMidDb, 0.8);
            pCr.bell (fs, std::clamp ((double) s.postCrunchHz, 1500.0, fs * 0.4), s.postCrunchDb, 0.9); pHigh.shelf (fs, std::min (9000.0, fs * 0.4), s.postHighDb, true);
            if (s.lpHz < 19999.f) { const double lh = std::clamp ((double) s.lpHz, 500.0, fs * 0.45); lp1.lp (fs, lh, 0.5412); lp2.lp (fs, lh, 1.3066); }   // 4th order Butterworth
            if (s.harmDb != 0.f) eHarm.bell (fs, std::clamp (endHz * (double) std::clamp (s.harmN + 1, 1, 4), 20.0, fs * 0.4), s.harmDb, 1.4);
            clickDecay = (float) std::exp (-1.0 / (fs * 0.001 * std::max (0.05f, s.clickLenMs)));
            clickBalNow = std::clamp (s.clickBal, 0.f, 1.f);
            fdA.cook (s.fd1); fdB.cook (s.fd2);
            cAtk = 1.f - (float) std::exp (-1.0 / (fs * 0.001 * std::max (0.1f, s.compAtkMs)));
            cRel = 1.f - (float) std::exp (-1.0 / (fs * 0.001 * std::max (1.f, s.compRelMs)));
            { const float thr = 1.f - 0.8f * std::clamp (s.compAmt, 0.f, 1.f); cMake = 1.f / (thr + (1.f - thr) * 0.25f); }
            crHp.hp (fs, 150.0, 0.7071); crLp.lp (fs, std::min (9000.0, fs * 0.45), 0.7071);
            txBp.bp (fs, 3500.0, 1.0);
            pnBp.bp (fs, std::clamp ((double) s.pnHz, 100.0, fs * 0.45), 0.8);
        }

        double fs = 48000.0, p = 0.0, pc = 0.0, hz = 100.0, endHz = 50.0;
        int blk = 0, crushN = 0, pnN = 0; float pnHeld = 0.f;
        uint32_t seed = 0x5eedbeefu;
        float crNow = 0.f;
    public:
        float lastCrunch() const { return crNow; }               // LAYER ENERGY: the crunch of this sample
    private:
        double pm = 0.0;
        float oc = 1.f, gClip2 = 1.f, fuzzEnv = 0.f, cEnv = 0.f, cAtk = 0.01f, cRel = 0.001f, cMake = 1.f;
        bq::Biquad lp1, lp2, eHarm;
        fastdist::Stage fdA, fdB;
        float clickEnv = 0.f, clickSign = 1.f, clickDecay = 0.99f, clickBalNow = 0.f;
        float preEnv = 1.f, segOut = 1.f;
        float segGain = 1.f, outEnv = 1.f, g1 = 1.f, g2 = 1.f, gc = 1.f, gPos = 1.f, gNeg = 1.f, rms = 0.05f, crush = 0.f;
        bq::Biquad eLow, eLm, eMid, eHm, eHigh, dc1, dcOut, betw, pLow, pBody, pMid, pCr, pHigh, crHp, crLp, txBp, pnBp;
    };
}
