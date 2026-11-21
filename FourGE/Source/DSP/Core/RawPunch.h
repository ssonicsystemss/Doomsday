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
//   DISTORTED CRUNCH = a CRACK IMPULSE ENGINE (owner 2026-11-21 rework, RAW_CRUNCH.md: "CZ-KR-CZ-KRR", a series of distorted
//      impulses, NEVER a continuous noise): a parallel path of the punch. IMPULSE SEQUENCE locked to the punch's own cycles (rhythmic
//      with it), irregular but repeatable (a fixed hash, no random generator), the first impulse the strongest, the next ones weaker;
//      DENSITY = how close they come. Each impulse: a short tonal burst (saw / square + the punch's tone; SNARE / NOISE / HYBRID only
//      colours it) -> PRE-SHAPING -> ASYMMETRIC WAVESHAPING (GRIT) -> MULTISTAGE SATURATION -> HARD CLIP -> CRUNCH TONE -> ITS OWN
//      ENVELOPE after the distortion (so the impulse stays an impulse) -> the SEQUENCE MIX -> CRUNCH. The whole series lives from the
//      punch start to PUNCH END + BODY DURATION x BODY FOLLOW, fading toward the end; inside the body the impulses get softer and
//      darker (BODY TRANSITION), no level step.
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
            p = 0.0; blk = 0; seed = 0x7a3c19e5u; envIn = envOut = 0.f; follow = 0.f; impCount = 0; sinceImp = 0.f; lastP = 0.0;
            for (auto& v : imp) { v.on = false; v.lp.clear(); v.hp.clear(); }
            cOut1.clear(); cOut2.clear();
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

        // ---------------- DISTORTED CRUNCH: the CRACK IMPULSE ENGINE (a parallel path of the punch)
        static float hashU (uint32_t k)                       // a fixed, repeatable "irregularity" (-1..1): the same kick = the same series
        {
            k ^= k >> 16; k *= 0x7feb352du; k ^= k >> 15; k *= 0x846ca68bu; k ^= k >> 16;
            return (float) (k & 0xffffff) / 8388607.5f - 1.f;
        }
        float crunchPath (const Settings& s, float ms, float src, float n)
        {
            const float pEnd = std::max (10.f, s.decayMs), end = crunchEnd (s);
            if (ms > end + 10.f) return 0.f;
            // the series' own envelope: full while the punch lasts, then it fades through the body (BODY FOLLOW); 0 at the end
            const float seriesEnv = ms < pEnd ? 1.f - 0.25f * ms / pEnd : std::max (0.f, 0.75f * std::pow (1.f - (ms - pEnd) / std::max (8.f, end - pEnd), 1.3f));
            const float inBody = std::clamp ((ms - std::max (pEnd, s.bodyStartMs)) / std::max (8.f, end - pEnd), 0.f, 1.f);
            // ---- IMPULSE SEQUENCE: fire on a punch cycle once the (irregular) interval has passed - rhythmic with the punch
            const bool wrap = p < lastP;
            lastP = p;
            sinceImp += 1000.f / (float) fs;
            const float baseMs = 9.f - 6.5f * std::clamp (s.crunchDensity, 0.f, 1.f);               // DENSITY: ~9 ms .. ~2.5 ms apart
            const float want = baseMs * (1.f + 0.45f * hashU (0x51u + (uint32_t) impCount * 2654435761u));
            if (ms < end && (impCount == 0 || (wrap && sinceImp >= want)))
            {
                auto& v = imp[(size_t) (impCount % (int) imp.size())];
                const float r1 = hashU (0xA3u + (uint32_t) impCount * 40503u), r2 = hashU (0x7Fu + (uint32_t) impCount * 9973u);
                v.on = true; v.t = 0.f; v.ph = 0.0;
                v.amp = (impCount == 0 ? 1.f : std::clamp (0.62f - 0.03f * (float) impCount + 0.15f * r1, 0.2f, 0.75f)) * seriesEnv;   // the first the strongest
                v.hz = (700.0 + 2600.0 * s.crunchTone) * (1.0 + 0.25 * r2) * (1.0 - 0.45 * inBody);
                v.dec = impCount == 0 ? 2.2f + 1.2f * (1.f - s.crunchTone)                         // ms: the first a bit longer, the rest short -
                                      : std::min (1.0f + 1.2f * (1.f - s.crunchTone) + 0.8f * inBody, 0.3f * want);   // never smeared into each other
                v.lp.lp (fs, std::clamp ((4500.0 + 9000.0 * s.crunchTone) * (1.0 - 0.6 * inBody), 1200.0, fs * 0.45), 0.7071);   // BODY TRANSITION: less sharp
                v.hp.hp (fs, 350.0, 0.7071);
                ++impCount; sinceImp = 0.f;
            }
            // ---- each impulse: a tonal burst -> distortion -> CRUNCH TONE -> ITS OWN ENVELOPE (after the distortion: it stays an impulse)
            const float g = 2.5f + 14.f * s.grit;
            float out = 0.f;
            for (auto& v : imp)
            {
                if (! v.on) continue;
                const float e = v.t < 0.15f ? v.t / 0.15f : std::exp (-(v.t - 0.15f) / v.dec);
                if (v.t > 0.15f + 7.f * v.dec) { v.on = false; continue; }
                const double ph = v.ph; v.ph += v.hz / fs; if (v.ph >= 1.0) v.ph -= 1.0;
                const float saw = (float) (2.0 * ph - 1.0), sq = ph < 0.5 ? 1.f : -1.f;
                float x = 0.55f * saw + 0.45f * sq + 0.5f * src;                                   // harmonic material (+ the punch's tone)
                if (s.crunchSource != 2) x += (s.crunchSource == 1 ? 0.35f : 0.2f) * n;           // NOISE / SNARE only colour it (never the source)
                x = v.hp.tick (x);                                                                 // PRE-SHAPING: no lows (no second DUM)
                x = x >= 0.f ? 1.f - std::exp (-g * x) : -(1.f - std::exp (g * 0.55f * x)) * 0.62f;   // ASYMMETRIC WAVESHAPING
                for (int k = 0; k < 2 + (int) (s.grit * 2.f); ++k) x = std::tanh (x * 2.2f) * 0.92f;  // MULTISTAGE SATURATION
                x = std::clamp (x * 1.5f, -0.8f, 0.8f) / 0.8f;                                     // HARD CLIP
                x = v.lp.tick (x);                                                                 // CRUNCH TONE
                out += x * e * v.amp;                                                              // IMPULSE ENVELOPE (+ the sequence mix)
                v.t += 1000.f / (float) fs;
            }
            // no lows: the impulse RATE itself (and the asymmetric shaper's DC) would make a low thump - cut after the sequence mix
            out = cOut2.tick (cOut1.tick (out));
            return out * s.crunch * 3.2f;
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
            cOut1.hp (fs, 300.0, 0.7071); cOut2.hp (fs, 300.0, 0.7071);
            bridgeLp.lp (fs, std::clamp (2200.0 - 1800.0 * std::clamp (ms / (cd * 3.f + 30.f), 0.f, 1.f), 250.0, 2400.0), 0.7071);
            dcb.hp (fs, 18.0, 0.7071);
            driveG = std::pow (10.f, s.driveDb / 20.f);
        }

        double fs = 48000.0, p = 0.0, hz = 100.0;
        int blk = 0, folds = 1;
        uint32_t seed = 0x7a3c19e5u;
        float punchEnv = 1.f, crackEnv = 0.f, bridgeEnv = 0.f, crackG = 1.f, driveG = 1.f, envIn = 0.f, envOut = 0.f, follow = 0.f;
        struct Impulse { bool on = false; float t = 0.f, amp = 0.f, dec = 1.f; double ph = 0.0, hz = 1000.0; bq::Biquad lp, hp; };
        std::array<Impulse, 6> imp;
        int impCount = 0;
        float sinceImp = 0.f;
        double lastP = 0.0;
        bq::Biquad cOut1, cOut2;
        bq::Biquad pre1, pre2, band1, band2, bridgeLp, tilt, tilt2, edgeHp, snapBp, dcb, outLp;
    };
}
