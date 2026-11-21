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
//  MODEL TRANSIENT CRACK (owner 2026-11-21 REWORK, the default): ONE short event "KRAK!" (2-25 ms), never a layer / noise / tail:
//    SOURCE (IMPULSE / CLICK / SHORT SAW / SHORT SQUARE / NOISE BURST / HYBRID = 70 % impulse + saw, 30 % a very short burst), its own pitch
//    (the punch x OFFSET, a fast drop inside the length) -> PRE GAIN -> WAVESHAPER (ASYMMETRIC: f(x) != -g(-x)) -> BANDPASS (follows the
//    pitch) -> HARD CLIP; + CRACK BODY (5-40 ms, lightly distorted). CRACK POSITION inside the punch, CRACK WIDTH = its length, RAW CRACK
//    = character (amplitude, density, asymmetry, sharpness; length barely), the PUNCH level scales it, SOLO CRACK = only this.
//  Runs at the engine's oversampled rate (the nonlinear stages are oversampled). DSP only; reset() per kick, no allocation.
// =====================================================================
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "../Tone/Biquad.h"
#include "CrackReverb.h"

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
        // TRANSIENT CRACK (owner 2026-11-21 REWORK, RAW_CRACK.md): model 0 = this, 1 = the CLASSIC LAYER above
        int model = 0, source = 5, ws = 2;    // HYBRID, ASYMMETRIC
        float raw = 0.5f, position = 0.5f, width = 0.5f, pitchOff = 9.f, pitchEnv = 12.f, asym = 0.6f;
        float bpHz = 3000.f, bpWidth = 2.f, bpRes = 0.3f, cbody = 0.3f;
        float punchAmt = 1.f;                 // CRACK follows the PUNCH (its level): PUNCH 0 = no crack
        int listen = 0;                       // KICK / SOURCE / DRY / WET / REVERB / FINAL
        // CRACK REVERB (owner 2026-11-21): ROUTING 0 PRE / 1 POST / 2 PARALLEL
        bool rvOn = true, rvGate = true; int rvRoute = 2, rvDriveType = 2;
        float rvMix = 0.4f, rvSize = 0.4f, rvDecayMs = 120.f, rvPreMs = 2.f, rvEarly = 0.7f, rvLate = 0.5f, rvDiffusion = 0.5f, rvDamping = 0.4f;
        float rvDrive = 0.3f, rvHpHz = 400.f, rvLpHz = 7000.f, rvRes = 0.1f;
        float hiGainDb = 0.f, hiHz = 5000.f;  // HI EQ: a high shelf on the whole crack (dry + wet)
    };
    // TRANSIENT CRACK timing: where it starts (CRACK POSITION: 0 = the punch start, 1 = the body start) and how long it lives
    inline float tcStartMs (const CrackSettings& s) { return std::clamp (s.position, 0.f, 1.f) * std::max (0.f, s.bodyStartMs); }
    inline float tcLenMs (const CrackSettings& s)        // CRACK WIDTH: 0 = 2.5 ms, 50 % = ~10 ms, 100 % = 22 ms (RAW CRACK lengthens it only a little)
    { return (2.5f + 19.5f * std::pow (std::clamp (s.width, 0.f, 1.f), 1.3f)) * (0.9f + 0.2f * std::clamp (s.raw, 0.f, 1.f)); }
    inline float tcBodyMs (const CrackSettings& s) { return (5.f + 35.f * std::clamp (s.width, 0.f, 1.f)) * (0.9f + 0.2f * std::clamp (s.raw, 0.f, 1.f)); }

    class CrackUnit
    {
    public:
        void prepare (double rate) { fs = rate; rev.prepare (rate); reset(); }
        void reset()
        {
            p = 0.0; ps = 0.0; mph = 0.0; pb = 0.0; tcHz = 0.0; seed = 0x6a09e667u; blk = 0; shN = 0; sh = 0.f;
            for (auto* b : { &lc, &loSh, &midB, &hiSh, &slideLp, &snapBp, &edgeHp, &pre1, &cpLp, &bodyHp, &bodyLp, &rvHp, &rvLp, &rvPre, &tcHi }) b->clear();
            dDry.clear(); dMain.clear(); dWet.clear(); rev.clear(); rvBlk = 0;
        }
        float layerEndMs (const CrackSettings& s) const
        {
            if (s.model == 0)
                return tcStartMs (s) + std::max ({ tcLenMs (s), s.cbody > 0.f ? tcBodyMs (s) : 0.f,
                                                   tcLenMs (s) + (s.rvOn && s.rvMix > 0.f ? s.rvPreMs + s.rvDecayMs * (s.rvGate ? 0.35f : 1.5f) : 0.f) }) + 2.f;
            return std::max (endMs (s) + s.releaseMs, s.snapLenMs * 3.f) + 60.f * s.coupling + 5.f;
        }
        // COUPLING: the crack's end is synced so it always reaches INTO the body's start (no hole between KRAK and the body)
        static float endMs (const CrackSettings& s) { const float sync = std::max (s.lengthMs, s.bodyStartMs + 1.5f * s.decayMs); return s.lengthMs + (sync - s.lengthMs) * std::clamp (s.coupling, 0.f, 1.f); }

        // punch = the RAW punch sample, punchHz = its pitch now, body = the BODY bus sample (raw), noteHz = the note
        float process (const CrackSettings& s, float ms, float punch, float punchHz, float body, float noteHz)
        {
            if (! s.on || s.gain <= 0.f || ms > layerEndMs (s)) return 0.f;
            if (s.model == 0) return transient (s, ms, punchHz);
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
        // ---------------- TRANSIENT CRACK: SOURCE -> [CRACK REVERB by ROUTING] -> WAVESHAPER -> BANDPASS -> HARD CLIP (+ CRACK BODY)
        //   ROUTING  PRE:      SOURCE -> PRE-EQ -> REVERB (dry / wet by MIX) -> DISTORTION -> FINAL ENVELOPE (distorted reflections)
        //            POST:     SOURCE -> DISTORTION -> REVERB (dry / wet by MIX) -> FINAL ENVELOPE (space, reflections not re-distorted)
        //            PARALLEL: DRY = SOURCE -> DISTORTION;  WET = SOURCE -> PRE-EQ -> REVERB -> FILTER -> REVERB DRIVE;  DRY + MIX x WET
        //   the wet path always: REVERB -> REVERB FILTER (HP / LP / RES) -> REVERB DRIVE. MIX 0 / REVERB OFF = the dry crack, unchanged.
        //   LISTEN: KICK (normal) / SOURCE / DRY / WET / REVERB (raw) / FINAL - the engine plays only the crack for the SOLO ones.
        struct Dist                            // one distortion stage (its own filter states): WAVESHAPER -> BANDPASS -> HARD CLIP
        {
            bq::Biquad hp, hp2, lp;
            void clear() { hp.clear(); hp2.clear(); lp.clear(); }
            float run (const CrackSettings& s, float x, float gain, float a, float clipG)
            {
                const float v = x * gain;
                float y;
                switch (s.ws)
                {
                    case 0:  y = std::tanh (v); break;                                                              // SOFT
                    case 1:  y = std::clamp (v, -(1.f - 0.5f * a), 1.f); break;                                     // HARD (its rails differ)
                    case 3:  y = std::sin (0.7f * v + 0.5f * a); break;                                             // FOLD (+ bias)
                    case 4:  { const float q = std::tanh (v); y = (1.f - a) * q + a * std::abs (q); break; }        // RECTIFY
                    default: y = v >= 0.f ? std::tanh (v) : std::tanh (v * (1.f + 2.5f * a)) * (1.f - 0.55f * a); break;   // ASYMMETRIC: f(x) != -g(-x)
                }
                y = lp.tick (hp2.tick (hp.tick (y)));                                                               // BANDPASS (the attack band)
                return std::clamp (y * clipG, -1.f, 1.f);                                                           // HARD CLIP
            }
        };
        static float rvDrive (int type, float x, float d)                     // REVERB DRIVE: thickens the reflections (not a volume knob)
        {
            if (d <= 0.f) return x;
            const float g = 1.f + 19.f * d, norm = 1.f / std::max (1.f, std::tanh (g) * 1.2f);
            float y;
            switch (type)
            {
                case 1:  y = std::clamp (x * g, -1.f, 1.f); break;                                                  // HARD CLIP
                case 2:  { const float v = x * g; y = v >= 0.f ? std::tanh (v) : 0.6f * std::tanh (2.f * v); break; }   // ASYMMETRIC
                case 3:  y = std::sin (x * g * 0.8f); break;                                                         // FOLD
                default: y = std::tanh (x * g); break;                                                               // SOFT
            }
            return y * norm * (1.f + 0.5f * d);
        }
        float transient (const CrackSettings& s, float ms, float punchHz)
        {
            const float t = ms - tcStartMs (s);
            if (t < 0.f) { tcHz = 0.0; return 0.f; }
            const float len = tcLenMs (s), blen = tcBodyMs (s);
            const bool rvOn = s.rvOn && s.rvMix > 0.f;
            const float tail = rvOn ? s.rvPreMs + s.rvDecayMs * (s.rvGate ? 0.35f : 1.5f) : 0.f;
            const float life = std::max ({ len, s.cbody > 0.f ? blen : 0.f, len + tail });
            if (t > life) return 0.f;
            if (tcHz <= 0.0) tcHz = std::clamp ((double) (punchHz > 0.f ? punchHz : 300.f), 40.0, 4000.0);   // the punch's pitch when the crack starts
            const float raw = std::clamp (s.raw, 0.f, 1.f), rk = std::pow (raw, 1.2f);
            // its OWN pitch: PUNCH x PITCH OFFSET, a fast move from + PITCH DROP to 0 inside the crack's length (+12 -> +3 -> 0)
            const double st = s.pitchOff + s.pitchEnv * std::exp (-3.0 * std::min (t, len) / len);
            const double hz = std::clamp (tcHz * std::exp2 (st / 12.0), 40.0, fs * 0.2);
            if (blk-- <= 0) { blk = 16; cookTc (s, hz); }
            const double ph = p; p += hz / fs; if (p >= 1.0) p -= 1.0;
            // ---- SOURCE (with its envelope: ATTACK 0, DECAY = the length, a hard termination)
            float src = 0.f, saw = 0.f;
            if (t < len)
            {
                const float imp = ph < 0.06 ? 1.f : (ph < 0.12 ? -0.5f : 0.f);                 // a narrow pulse (an impulse per cycle)
                saw = (float) (2.0 * ph - 1.0); const float sq = ph < 0.5 ? 1.f : -1.f;
                const float click = (float) std::sin (6.283185307 * 4.0 * tcHz * t * 0.001) * std::exp (-t / 0.35f);
                const float nb = noise() * std::exp (-t / std::max (0.3f, len * 0.15f));      // a VERY short noise burst
                switch (s.source)
                {
                    case 0:  src = imp; break;
                    case 1:  src = click; break;
                    case 2:  src = saw; break;
                    case 3:  src = sq; break;
                    case 4:  src = noise(); break;
                    default: src = 0.7f * (0.5f * imp + 0.5f * saw) + 0.3f * nb; break;           // HYBRID: 70 % impulse / shaped wave + 30 % noise
                }
                src *= std::exp (-t / (len * 0.28f)) * std::clamp ((len - t) / 0.15f, 0.f, 1.f);
            }
            const float a = std::clamp (s.asym + 0.3f * raw, 0.f, 1.f), gain = 1.5f + 22.f * rk, clipG = 1.6f + 1.2f * raw;
            const float dryEnv = std::exp (-t / (len * 0.45f)) * std::clamp ((len - t) / 0.15f, 0.f, 1.f);   // the dry crack vanishes at once
            // ---- the DRY crack (always computed: the LISTEN modes + PARALLEL / POST)
            const float dry = t < len ? dDry.run (s, src, gain, a, clipG) * dryEnv : 0.f;
            float out = dry, rvRaw = 0.f, wet = 0.f;
            if (rvOn)
            {
                cookRv (s);
                const float mix = std::clamp (s.rvMix, 0.f, 1.f);
                const float gate = s.rvGate ? std::clamp ((len + s.rvPreMs + 0.35f * s.rvDecayMs - t) / 1.f, 0.f, 1.f) : 1.f;   // GATE: cut while still loud (~ -20 dB)
                auto filt = [&] (float x) { return rvLp.tick (rvHp.tick (x)); };
                if (s.rvRoute == 0)            // PRE-DISTORTION: the reflections go INTO the distortion
                {
                    const float pin = rvPre.tick (src);
                    rvRaw = rev.process (pin) * gate;
                    const float w = rvDrive (s.rvDriveType, filt (rvRaw), s.rvDrive);
                    const float in = (1.f - mix) * src + mix * 2.5f * w;                       // (x2.5: a reverb is far below its source)
                    // FINAL ENVELOPE: it follows the source, then the reverb's own decay (never a held noise)
                    const float fenv = t < len ? dryEnv + (1.f - dryEnv) * mix * std::exp (-t / std::max (2.f, s.rvDecayMs * 0.25f))
                                               : mix * std::exp (-(t - len) / std::max (2.f, s.rvDecayMs * 0.25f)) * std::exp (-len / std::max (2.f, s.rvDecayMs * 0.25f)) * gate;
                    wet = dWet.run (s, 2.5f * w, gain, a, clipG) * fenv;                       // (LISTEN WET: the distorted reflections alone)
                    out = dMain.run (s, in, gain, a, clipG) * std::max (fenv, t < len ? dryEnv : 0.f);
                }
                else if (s.rvRoute == 1)       // POST-DISTORTION: space on the finished crack, the reflections not re-distorted
                {
                    rvRaw = rev.process (dry) * gate;
                    wet = rvDrive (s.rvDriveType, filt (rvRaw), s.rvDrive) * 2.f;
                    out = dry * (1.f - 0.5f * mix) + mix * wet;
                }
                else                           // PARALLEL: the dry crack untouched + the wet path
                {
                    rvRaw = rev.process (rvPre.tick (src)) * gate;
                    wet = rvDrive (s.rvDriveType, filt (rvRaw), s.rvDrive) * 2.f;
                    out = dry + mix * wet;
                }
            }
            if (s.hiGainDb > 0.05f) out = tcHi.tick (out);                                      // HI EQ (the shelf is cooked in cookTc)
            const float amp = (0.45f + 0.75f * raw) * std::clamp (s.punchAmt, 0.f, 1.25f);
            out *= amp;
            // CRACK BODY: 5-40 ms, very lightly distorted, below the crack (the "micro crunch" after the KRAK, never a 2nd punch)
            if (s.cbody > 0.f && t < blen)
            {
                pb += 0.5 * hz / fs; if (pb >= 1.0) pb -= 1.0;
                const float bt = std::clamp ((blen - t) / 0.3f, 0.f, 1.f);
                float b = (float) std::sin (6.283185307 * pb) + 0.3f * (float) (2.0 * pb - 1.0);
                b = std::tanh (1.6f * b) / std::tanh (1.6f);
                b = bodyLp.tick (bodyHp.tick (b)) * std::exp (-t / (blen * 0.35f)) * bt;
                out += b * s.cbody * 0.6f * std::clamp (s.punchAmt, 0.f, 1.25f);
            }
            switch (s.listen)                  // LISTEN (diagnostics): what the SOLO plays
            {
                case 1: out = src * 0.7f; break;
                case 2: out = dry * amp; break;
                case 3: out = wet * amp; break;
                case 4: out = rvRaw * 2.f; break;
                default: break;
            }
            out *= s.gain;
            const float o = std::abs (out);
            return o > 0.95f ? std::copysign (0.95f + 0.05f * std::tanh ((o - 0.95f) / 0.05f), out) : out;
        }
        void cookRv (const CrackSettings& s)
        {
            if (rvBlk-- > 0) return;
            rvBlk = 32;
            CrackReverbSettings r; r.size = s.rvSize; r.decayMs = s.rvDecayMs; r.preDelayMs = s.rvPreMs; r.early = s.rvEarly; r.late = s.rvLate;
            r.diffusion = s.rvDiffusion; r.damping = s.rvDamping;
            rev.cook (r);
            const double q = 0.7071 + 3.0 * s.rvRes;
            rvHp.hp (fs, std::clamp ((double) s.rvHpHz, 20.0, fs * 0.3), q);
            rvLp.lp (fs, std::clamp ((double) s.rvLpHz, 500.0, fs * 0.45), q);
            rvPre.hp (fs, 250.0, 0.7071);                                                        // PRE-EQ: no lows into the reverb (no sub from it)
        }
        void cookTc (const CrackSettings& s, double hz)
        {
            const double w = std::clamp ((double) s.bpWidth, 0.5, 4.0);
            const double fc = std::clamp ((double) s.bpHz * std::clamp (std::sqrt (hz / 500.0), 0.6, 1.8), 500.0, fs * 0.35);
            const double q = 0.7071 + 2.5 * s.bpRes + 0.8 * s.raw;                               // RESONANCE (+ RAW CRACK = a sharper transient)
            const double lo = std::clamp (fc / std::exp2 (w * 0.5), 150.0, fs * 0.3), hi = std::clamp (fc * std::exp2 (w * 0.5), 800.0, fs * 0.45);
            for (auto* d : { &dDry, &dMain, &dWet }) { d->hp.hp (fs, lo, q); d->hp2.hp (fs, lo, 0.7071); d->lp.lp (fs, hi, q); }
            bodyHp.hp (fs, 120.0, 0.7071); bodyLp.lp (fs, 2500.0, 0.7071);
            tcHi.shelf (fs, std::clamp ((double) s.hiHz, 1000.0, fs * 0.4), s.hiGainDb, true);
        }
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

        double fs = 48000.0, p = 0.0, ps = 0.0, mph = 0.0, toneNow = 1000.0, snapHz = 400.0, pb = 0.0, tcHz = 0.0;
        uint32_t seed = 0x6a09e667u;
        int blk = 0, folds = 1, shN = 0;
        float preG = 1.f, clipAmt = 0.f, crackLevel = 0.f, sh = 0.f;
        bq::Biquad lc, loSh, midB, hiSh, slideLp, snapBp, edgeHp, pre1, cpLp, bodyHp, bodyLp, rvHp, rvLp, rvPre, tcHi;
        Dist dDry, dMain, dWet;
        CrackReverb rev;
        int rvBlk = 0;
    };
}
