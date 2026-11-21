#pragma once
// =====================================================================
//  4GE - PUNCH TYPE RAW (owner 2026-11-21, FourGE/RAW_PUNCH.md): TWO independent architectures, never one engine with other values.
//   RAW CLASSIC                TOK / TRANSIENT -> PUNCH -> CRACK LAYER -> BODY -> TAIL      "KRAK -> DUM"
//      PUNCH SOURCE (own pitch / envelope / harmonic density) + SNAP (the physical start)
//      CLASSIC CRACK = SOURCE -> PRE-SHAPE -> WAVESHAPER -> HARD CLIP -> BAND SHAPE -> CRACK ENVELOPE (short, focused, never the body)
//      BODY COUPLING: 0 = the crack ends before the body, 100 = the crack melts into a low-mid bridge (its band slides down into the body)
//   RAW 2015-2017 / CRACKING  SNAP -> CRACK GENERATOR -> HARMONIC SHAPER -> BODY COUPLING -> BODY -> TAIL   "KRAK! -> BRUTAL BODY"
//      TRANSIENT SOURCE -> SNAP SHAPER -> FAST SATURATION -> CRACK GENERATOR (tilt DARK / BRIGHT on the crack source, DENSITY folds,
//      CONTROLLED HARMONIC DISTORTION, EDGE = the upper harmonics, DYNAMIC response, level-held: DENSITY never raises the punch level)
//      BODY COUPLING: a distorted bridge that carries the crack's energy into the body (no hole between KRAK and the body)
//   SNAP = how hard the very start hits; CRACK = how harmonically aggressive that start is (a separate harmonic process, not a drive).
//   Distortion is a tool in several places (punch / crack / bridge), never "the whole kick -> distortion".
//   DYNAMIC DISTORTED CRUNCH (owner 2026-11-21, RAW_CRUNCH.md): a PARALLEL path of the same punch source - "CZZZZ / GRRRR", a held,
//      dense distorted grind that runs from the punch into the body (never a click, never a second "DUM"):
//      SOURCE (the punch's tone + the SNARE / NOISE / HYBRID broadband source) -> WINDOW -> PRE-SHAPING (no lows) -> ASYMMETRIC
//      WAVESHAPING -> MULTISTAGE SATURATION (EQ between the stages) -> CONTROLLED HARD CLIP -> TONE FILTER -> DYNAMIC ENVELOPE
//      (start = the punch, held to the PUNCH END, then into the BODY: end = PUNCH END + BODY DURATION x BODY FOLLOW) -> HYBRID
//      coupling (in the body it loses its edge and turns rougher / grainier, no level step) -> level-held CRUNCH -> + the punch.
//  Mono. DSP only; reset() per kick, no allocation.
// =====================================================================
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <array>
#include "../Tone/Biquad.h"

namespace fge::dsp::rawpunch
{
    struct Settings
    {
        bool on = false;
        int type = 0;                         // 0 RAW CLASSIC, 1 RAW 2015-2017 / CRACKING
        float snap = 0.5f, crack = 0.5f, crackDecayMs = 28.f, coupling = 0.6f;
        float startHz = 700.f, dropMs = 22.f, endSt = 12.f, decayMs = 110.f, driveDb = 10.f;
        float density = 0.4f, crackHz = 1500.f;                 // CLASSIC: the source's harmonic density, the crack band
        float edge = 0.5f, tone = 0.5f, dynamic = 0.5f;         // CRACKING: EDGE, DARK..BRIGHT, dynamic response (density = the crack's DENSITY)
        float kickMs = 400.f;                                   // the kick length (CRACKING: the crack decay follows it)
        // DYNAMIC DISTORTED CRUNCH
        bool crunchOn = false; int crunchSource = 2;            // SNARE / NOISE / HYBRID
        float crunch = 0.6f, grit = 0.6f, crunchDensity = 0.5f, crunchTone = 0.5f, bodyFollow = 0.4f;
        float bodyStartMs = 0.f, bodyEndMs = 220.f;             // the BODY phase (its delay .. delay + length), read from its envelope
    };

    class Unit
    {
    public:
        void prepare (double rate) { fs = rate; reset(); }
        void reset()
        {
            p = 0.0; blk = 0; seed = 0x7a3c19e5u; envIn = envOut = 0.f; follow = 0.f; snPh = 0.0; holdN = 0; held = 0.f;
            for (auto* b : { &cpre1, &cpre2, &clowCut, &ctoneBell, &ctoneLp }) b->clear();
            for (auto& m : cmid) m.clear();
            for (auto* b : { &pre1, &pre2, &band1, &band2, &bridgeLp, &tilt, &tilt2, &edgeHp, &snapBp, &dcb, &outLp }) b->clear();
        }

        float process (const Settings& s, float ms, float noteHz)
        {
            const float total = std::max ({ s.decayMs, s.crackDecayMs * 4.f, s.crunchOn ? crunchEnd (s) + 5.f : 0.f }) + 140.f * s.coupling + 20.f;
            if (! s.on || ms > total) return 0.f;
            if (blk-- <= 0) { blk = 16; cook (s, ms, noteHz); }
            // the PUNCH SOURCE: its own tonal oscillator on its own pitch envelope (sine -> rounded square with DENSITY)
            const double ph = p;
            p += hz / fs; if (p >= 1.0) p -= 1.0;
            const float sn = (float) std::sin (6.283185307 * ph);
            const float dens = s.type == 0 ? s.density : 0.1f;            // (CRACKING: a clean tonal source - the CRACK GENERATOR makes the harmonics)
            const float src = std::tanh (sn * (1.f + 6.f * dens)) / std::tanh (1.f + 6.f * dens);
            const float n = noise();
            const float y = s.type == 0 ? classic (s, ms, src, n) : cracking (s, ms, src, n);
            return s.crunchOn && s.crunch > 0.f ? y + crunchPath (s, ms, src, n) : y;
        }
        // CRUNCH_END = PUNCH_END + BODY_DURATION x BODY FOLLOW (the real phases, not fixed times)
        static float crunchEnd (const Settings& s)
        {
            const float punchEnd = std::max (10.f, s.decayMs);
            return punchEnd + std::max (0.f, s.bodyEndMs - std::max (s.bodyStartMs, 0.f)) * std::clamp (s.bodyFollow, 0.f, 1.f);
        }

        float lastHz() const { return (float) hz; }             // (the crack layer follows the punch's pitch)

    private:
        // ---------------- RAW CLASSIC
        float classic (const Settings& s, float ms, float src, float n)
        {
            // SNAP: a short knock + a fast attack with an overshoot (how hard the start hits)
            const float atkMs = 2.5f - 2.4f * s.snap;
            const float atk = ms < atkMs ? std::sin (1.5707963f * ms / atkMs) : 1.f;
            const float over = 1.f + 1.6f * s.snap * std::exp (-ms / 1.2f);
            const float knock = s.snap * 0.9f * snapBp.tick (n + 2.f * src) * std::exp (-ms / (0.6f + 0.8f * s.snap));
            float punch = src * atk * over * punchEnv;
            punch = std::tanh (punch * driveG) / std::tanh (driveG);                          // PUNCH DISTORTION (a little, the punch's own)
            // CLASSIC CRACK: source -> pre-shape -> waveshaper -> hard clip -> band shape -> crack envelope
            float c = pre2.tick (pre1.tick (src + 0.25f * n * std::exp (-ms / 4.f)));
            c = std::sin (c * crackG) ;                                                        // WAVESHAPER: a sine fold (harmonics, not loudness)
            c = std::clamp (c * (1.f + 2.f * s.crack), -0.8f, 0.8f) / 0.8f;                   // HARD CLIP
            c = band2.tick (band1.tick (c));                                                   // BAND SHAPE (+ the coupling slide)
            c *= crackEnv * s.crack * 1.4f;
            // BODY COUPLING: the crack melts into a low-mid bridge on the punch's pitch (KRAK -> DUM, not KRAK | DUM)
            const float bridge = bridgeLp.tick (std::tanh (src * 2.5f)) * bridgeEnv * s.coupling * 0.9f;
            return dcb.tick (0.7f * (punch + c + bridge) + knock);
        }
        // ---------------- RAW 2015-2017 / CRACKING
        float cracking (const Settings& s, float ms, float src, float n)
        {
            // TRANSIENT SOURCE -> SNAP SHAPER -> FAST SATURATION
            const float snapEnv = std::exp (-ms / (0.5f + 0.7f * s.snap));
            float sn = (0.6f * n + (src > 0.f ? 1.f : -1.f)) * snapEnv;
            sn = std::tanh (snapBp.tick (sn) * (2.f + 6.f * s.snap)) * (0.15f + 1.4f * s.snap);
            const float atkMs = 1.2f - 1.15f * s.snap;
            const float atk = ms < atkMs ? ms / atkMs : 1.f;
            const float punch = src * atk * punchEnv;
            // CRACK GENERATOR: the input (punch + snap) -> TILT (dark / bright, on the crack source) -> DENSITY folds -> CONTROLLED distortion
            const float in = tilt2.tick (tilt.tick (punch + 0.6f * sn));
            const float a = std::abs (in);
            follow += (a - follow) * (a > follow ? 0.3f : 0.004f) * (float) (176400.0 / fs);
            const float dyn = 1.f + s.dynamic * (std::min (2.f, follow * 2.f) - 1.f);         // DYNAMIC: harder input -> more crack
            float c = in * crackG * dyn;
            for (int k = 0; k < folds; ++k) c = std::sin (c * (1.2f + 0.6f * (float) k));     // DENSITY: 1 .. 4 fold stages = harmonic density
            c = c >= 0.f ? std::tanh (1.6f * c) : 0.8f * std::tanh (1.1f * c);                 // CONTROLLED HARMONIC DISTORTION (asymmetric)
            const float e = std::clamp (edgeHp.tick (c) * (2.f + 10.f * s.edge), -1.f, 1.f);    // EDGE: the upper harmonics, hard
            c += s.edge * 0.6f * e;
            // level held: DENSITY / EDGE never raise the punch level (follow the input's envelope)
            const float ac = std::abs (c);
            envIn += (a - envIn) * 0.02f * (float) (176400.0 / fs); envOut += (ac - envOut) * 0.02f * (float) (176400.0 / fs);
            c *= std::clamp (envIn / std::max (1.0e-4f, envOut), 0.15f, 1.5f);
            c *= crackEnv * 1.9f * s.crack;
            // BODY COUPLING: a distorted bridge that carries the crack's energy into the body (no hole)
            const float bridge = bridgeLp.tick (std::tanh (src * driveG) / std::tanh (driveG) + 0.3f * c) * bridgeEnv * s.coupling * 1.1f;
            const float y = 0.7f * punch * (1.f - 0.45f * s.crack) + c + bridge;
            return dcb.tick (outLp.tick (std::tanh (y * 1.3f) / std::tanh (1.3f)) * 0.8f + 0.6f * sn);   // (the SNAP on top: never squashed by the crack's saturation)
        }

        // ---------------- DYNAMIC DISTORTED CRUNCH (a parallel path of the punch source)
        float crunchPath (const Settings& s, float ms, float src, float n)
        {
            const float pEnd = std::max (10.f, s.decayMs), end = crunchEnd (s);
            // DYNAMIC ENVELOPE: a fast attack (1 ms), HELD while the punch lasts, then it decays into the body (BODY FOLLOW)
            float env;
            if (ms < 1.f) env = ms;
            else if (ms < pEnd) env = 1.f - 0.15f * (ms - 1.f) / (pEnd - 1.f);
            else { const float u = (ms - pEnd) / std::max (8.f, end - pEnd); env = u >= 1.f ? 0.f : 0.85f * std::pow (1.f - u, 1.5f); }
            if (env <= 0.f) return 0.f;
            // SOURCE: the punch's own tone + the broadband SNARE / NOISE / HYBRID source (a held grain, not a decaying hit)
            snPh += 190.0 / fs; if (snPh >= 1.0) snPh -= 1.0;
            const float body = (float) std::sin (6.283185307 * snPh);
            float bb = s.crunchSource == 1 ? n : s.crunchSource == 0 ? 0.6f * n + 0.7f * body : 0.45f * n + 0.4f * body;
            float x = 0.8f * src + bb;
            // PRE-SHAPING: no lows (the body keeps the mass), the level into the shapers
            x = cpre2.tick (cpre1.tick (x));
            // ASYMMETRIC WAVESHAPING (GRIT = how hard)
            const float g = 2.f + 12.f * s.grit;
            x = x >= 0.f ? 1.f - std::exp (-g * x) : -(1.f - std::exp (g * 0.55f * x)) * 0.6f;
            // MULTISTAGE SATURATION (DENSITY = how many), an EQ between the stages so no stage piles up mud
            for (int k = 0; k < cStages; ++k) { x = cmid[(size_t) k].tick (x); x = std::tanh (x * (1.8f + 1.2f * s.crunchDensity)) * 0.9f; }
            // CONTROLLED HARD CLIP (independent of the kick's drive, a stable level)
            x = std::clamp (x * 1.4f, -0.75f, 0.75f) / 0.75f;
            // HYBRID coupling: inside the body it loses its edge and gets rougher / grainier (a sample-and-hold), no level step
            const float inBody = std::clamp ((ms - std::max (pEnd, s.bodyStartMs)) / std::max (8.f, end - pEnd), 0.f, 1.f);
            const int hold = 1 + (int) (inBody * (2.f + 10.f * s.grit) * (float) (fs / 176400.0));
            if (++holdN >= hold) { holdN = 0; held = x; }
            x = held;
            // TONE FILTER: dark .. bright, the aggressive mids up, no hiss (the top closes as it goes into the body)
            x = ctoneLp.tick (ctoneBell.tick (clowCut.tick (x)));
            // the level: after the hard clip the layer is level-stable, so CRUNCH = how much of it (never "more drive = louder")
            return x * env * s.crunch * 1.1f * (1.f - 0.35f * inBody);
        }
        float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f); }
        void cook (const Settings& s, float ms, float noteHz)
        {
            // pitch: START -> the punch's end (the note + END st) in DROP ms (an exponential-ish fall, faster in CRACKING)
            const double end = std::max (30.0, (double) (noteHz > 0.f ? noteHz : 49.f) * std::exp2 (s.endSt / 12.0));
            const double start = std::max (end, (double) s.startHz);
            const float u = std::clamp (ms / std::max (1.f, s.dropMs), 0.f, 1.f);
            const float k = s.type == 0 ? 1.8f : 3.f;
            hz = std::clamp (end * std::pow (start / end, (double) std::pow (1.f - u, k)), 20.0, fs * 0.3);
            // the envelopes
            punchEnv = std::pow (std::max (0.f, 1.f - ms / std::max (10.f, s.decayMs)), s.type == 0 ? 1.4f : 1.9f);
            float cd = s.crackDecayMs;
            if (s.type == 1) cd *= std::clamp (s.kickMs / 400.f, 0.6f, 1.3f);               // CRACKING: the crack follows the kick length
            const float hard = 1.f - s.coupling;                                              // COUPLING 0: the crack is gated off before the body
            crackEnv = std::exp (-ms / std::max (2.f, cd)) * (ms < cd * 1.6f ? 1.f : std::max (0.f, 1.f - hard * (ms - cd * 1.6f) / 3.f));
            if (s.type == 0) { if (ms < 0.3f) crackEnv *= ms / 0.3f; }
            else crackEnv *= std::clamp ((ms - 1.5f) / 2.f, 0.f, 1.f);                       // CRACKING: SNAP first, then the KRAK (the crack follows the snap)
            const float bStart = cd * (0.4f + 0.6f * (1.f - s.coupling)), bLen = 30.f + 110.f * s.coupling;
            bridgeEnv = ms < bStart ? (ms / std::max (0.5f, bStart)) * s.coupling : std::max (0.f, 1.f - (ms - bStart) / bLen);
            // filters
            pre1.hp (fs, 250.0, 0.7071);
            if (s.type == 0)
            {
                pre2.bell (fs, std::clamp ((double) s.crackHz, 400.0, 6000.0), 6.0, 0.9);
                // BAND SHAPE: the crack band - with COUPLING its top slides down as it decays (it melts into the body)
                const double slide = 1.0 - s.coupling * std::clamp (ms / std::max (5.f, cd * 2.f), 0.f, 1.f) * 0.85;
                band1.hp (fs, std::clamp (s.crackHz * 0.4 * slide, 120.0, 4000.0), 0.7071);
                band2.lp (fs, std::clamp (s.crackHz * 3.5 * slide, 500.0, fs * 0.45), 0.7071);
                crackG = 1.f + 9.f * s.crack;
                snapBp.bp (fs, 3200.0, 1.1);
            }
            else
            {
                tilt.shelf (fs, 600.0, -(s.tone - 0.5f) * 14.f, false);                        // DARK .. BRIGHT (a tilt around ~1 kHz), on the crack source
                tilt2.shelf (fs, 1800.0, (s.tone - 0.5f) * 14.f, true);
                edgeHp.hp (fs, 2800.0 + 3000.0 * s.tone, 0.7071);
                folds = 1 + (int) std::lround (3.f * std::clamp (s.density, 0.f, 1.f));          // LOW / MEDIUM / HIGH / EXTREME
                crackG = 1.5f + 10.f * s.crack;
                snapBp.bp (fs, 4500.0, 0.8);
                outLp.lp (fs, 9000.0 + 6000.0 * s.tone, 0.7071);
            }
            if (s.crunchOn)
            {
                cpre1.hp (fs, 260.0, 0.7071); cpre2.hp (fs, 260.0, 0.7071);
                cStages = 2 + (int) std::lround (3.f * std::clamp (s.crunchDensity, 0.f, 1.f));
                for (auto& m : cmid) m.hp (fs, 300.0, 0.6);
                const float pEnd = std::max (10.f, s.decayMs), end = crunchEnd (s);
                const float inBody = std::clamp ((ms - pEnd) / std::max (8.f, end - pEnd), 0.f, 1.f);
                clowCut.hp (fs, 400.0, 0.7071);
                ctoneBell.bell (fs, 900.0 + 2400.0 * s.crunchTone, 6.0, 0.9);
                ctoneLp.lp (fs, std::clamp ((5000.0 + 8000.0 * s.crunchTone) * (1.0 - 0.6 * inBody), 1500.0, fs * 0.45), 0.7071);
            }
            bridgeLp.lp (fs, std::clamp (2200.0 - 1800.0 * std::clamp (ms / (cd * 3.f + 30.f), 0.f, 1.f), 250.0, 2400.0), 0.7071);
            dcb.hp (fs, 18.0, 0.7071);
            driveG = std::pow (10.f, s.driveDb / 20.f);
        }

        double fs = 48000.0, p = 0.0, hz = 100.0;
        int blk = 0, folds = 1;
        uint32_t seed = 0x7a3c19e5u;
        float punchEnv = 1.f, crackEnv = 0.f, bridgeEnv = 0.f, crackG = 1.f, driveG = 1.f, envIn = 0.f, envOut = 0.f, follow = 0.f;
        double snPh = 0.0;
        int cStages = 2, holdN = 0;
        float held = 0.f;
        bq::Biquad cpre1, cpre2, clowCut, ctoneBell, ctoneLp;
        std::array<bq::Biquad, 5> cmid;
        bq::Biquad pre1, pre2, band1, band2, bridgeLp, tilt, tilt2, edgeHp, snapBp, dcb, outLp;
    };
}
