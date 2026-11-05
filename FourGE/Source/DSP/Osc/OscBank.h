#pragma once
// =====================================================================
//  4GE MODULE 03 - OSCILLATOR ENGINE: 4 independent oscillators per kick voice (sized for 8: maxOsc).
//  Each oscillator: its own phases (one per unison voice), waveform (SINE / TRI / SAW / SQUARE / PULSE band-limited with
//  polyBLEP, NOISE from the NoiseEngine, CUSTOM / WAVETABLE / HARMONIC from mipmapped tables), pitch (follows a part's
//  pitch or a fixed Hz) x octave / semitone / fine / ratio, UNISON 1..16 (own frequency + phase per voice, gain kept),
//  PHASE modes, FM (true frequency modulation from the 4 x 4 matrix + feedback, 1-sample matrix delay),
//  RING modulation (a multiplication - its own algorithm), HARD / SOFT SYNC (polyBLEP-corrected resets).
//  Output: routed INTO the parts (PUNCH / BODY / TAIL / SUB inputs, stereo for the tail), level per route.
//  Runs at the oversampled rate (the engine's 4x), deterministic (seeded), no allocation.
// =====================================================================
#include "Noise.h"
#include "Wavetable.h"

namespace fge::dsp
{
    constexpr int maxOsc = 8, numOsc = 4, maxUnison = 16;
    enum OscWave { WSine = 0, WTriangle, WSaw, WSquare, WPulse, WNoise, WCustom, WWavetable, WHarmonic };
    enum OscTrack { TrackPunch = 0, TrackBody, TrackTail, TrackSub, TrackFixed };
    enum PhaseMode { PhReset = 0, PhRandom, PhFree, PhSynced };
    enum UniMode { UniCentered = 0, UniSymmetric, UniAsymmetric, UniRandom };
    enum UniPhase { UPhReset = 0, UPhRandom, UPhFree };
    enum WtMode { WtStatic = 0, WtLinear, WtEnvelope, WtLfo, WtRandom };

    // the tables the oscillators read (built off the audio thread; raw pointers, owner keeps them alive)
    struct OscTables { std::array<const Table*, maxOsc> t {}; };

    struct OscSettings
    {
        bool on = false, audible = false;
        int wave = WSaw, noise = NoiseWhite, track = TrackTail;
        float fixedHz = 110.f, mul = 1.f;                 // octave x semitone x fine x FM ratio
        float gain = 0.5f, panL = 1.f, panR = 1.f;        // level (and INVERT sign) / constant-power pan
        int phaseMode = PhReset, phaseSrc = 0;
        float phase01 = 0.f, phaseRand = 1.f;
        float pw = 0.5f;
        int uni = 1, uniMode = UniCentered, uniPhase = UPhRandom;
        float detuneCt = 20.f, spread = 0.5f, blend = 1.f;
        uint32_t seed = 1;
        float fmFb = 0.f, fmEnv = 0.f, ringEnv = 0.f, decayMs = 150.f;
        int ringSrc = 0;                                  // 0 off, 1..4 osc, 5 sine
        float ringAmt = 1.f, ringMix = 1.f, ringHz = 440.f;
        int syncSrc = 0, syncMode = 0;                    // 0 off, 1..4 master
        float syncRatio = 1.f;
        std::array<float, 4> route {};                    // linear gains into PUNCH / BODY / TAIL / SUB
        int wtMode = WtStatic;
        float wtPos = 0.f, wtMorph = 0.f, wtStart = 0.f, wtEnd = 1.f, wtRateMs = 300.f;
        // unison layout (cooked): ratio and pan gains per voice, the gain of each voice
        std::array<float, maxUnison> uRatio {}, uL {}, uR {}, uGain {};
    };
    struct OscBankSettings
    {
        std::array<OscSettings, maxOsc> o {};
        float fm[maxOsc][maxOsc] {};                      // fm[a][b] = a modulates b's frequency
        bool any = false, tailStereo = false;
    };

    // a deterministic random generator for phases / scans (never std::rand)
    struct Rng
    {
        uint32_t s = 1;
        void seed (uint32_t v) { s = v * 747796405u + 2891336453u; if (s == 0) s = 1; }
        float next01() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.f / 16777216.f); }
    };

    // ---- cook helper: unison voice layout (frequency ratios, pans, gains) - message / audio thread, no allocation
    inline void layoutUnison (OscSettings& o)
    {
        const int n = juce::jlimit (1, maxUnison, o.uni);
        Rng r; r.seed (o.seed * 7919u + 17u);
        float sum2 = 0.f;
        for (int k = 0; k < n; ++k)
        {
            float x = 0.f;                                    // -1..1 position of the voice in the detune spread
            if (n > 1)
            {
                const float lin = 2.f * (float) k / (float) (n - 1) - 1.f;
                switch (o.uniMode)
                {
                    case UniSymmetric:  x = (k % 2 == 0 ? 1.f : -1.f) * (float) ((k + 2) / 2) / (float) ((n + 1) / 2); if (n % 2 == 1 && k == n - 1) x = 0.f; break;
                    case UniAsymmetric: x = std::pow ((lin + 1.f) * 0.5f, 1.8f) * 1.4f - 0.4f; break;   // more voices above the centre
                    case UniRandom:     x = r.next01() * 2.f - 1.f; break;
                    default:            x = lin; break;
                }
            }
            o.uRatio[(size_t) k] = std::exp2 (x * o.detuneCt / 1200.f);
            const float pan = juce::jlimit (-1.f, 1.f, n > 1 ? x * o.spread : 0.f);
            const float a = (pan + 1.f) * juce::MathConstants<float>::pi * 0.25f;
            o.uL[(size_t) k] = std::cos (a) * 1.41421356f; o.uR[(size_t) k] = std::sin (a) * 1.41421356f;
            const bool centre = n == 1 || std::abs (x) < 1.0e-4f || (n % 2 == 1 && k == (n - 1) / 2 && o.uniMode == UniCentered);
            o.uGain[(size_t) k] = centre ? 1.f : o.blend;
            sum2 += o.uGain[(size_t) k] * o.uGain[(size_t) k];
        }
        // the unison never raises the level: voices that start in phase (RESET) add up like amplitudes,
        // voices with their own phases like powers - normalised to ONE voice either way
        float sum1 = 0.f;
        for (int k = 0; k < n; ++k) sum1 += o.uGain[(size_t) k];
        const float norm = o.uniPhase == UPhReset ? 1.f / juce::jmax (1.0e-6f, sum1) : 1.f / std::sqrt (juce::jmax (1.0e-6f, sum2));
        for (int k = 0; k < n; ++k) o.uGain[(size_t) k] *= norm;
    }

    // ---- one oscillator inside one kick voice
    struct OscUnit
    {
        std::array<float, maxUnison> ph {};
        NoiseEngine nz;
        Rng rng;
        float out = 0.f, outPrev = 0.f, fbState = 0.f;   // last output (FM / ring sources), feedback smoothing
        float pending = 0.f;                             // polyBLEP correction for the previous sample (sync)
        float delayed = 0.f;                             // the one-sample output delay that carries the correction
        float ringPh = 0.f, wtRandNow = 0.f, wtRandTarget = 0.f, wtRandMs = 0.f;
        bool wrapped = false; float wrapFrac = 0.f;      // master: wrapped this sample (and where)
        float L = 0.f, R = 0.f;
    };

    class OscBankVoice
    {
    public:
        // phases at the hit: RESET / RANDOM (seeded, a new value each hit) / FREE RUN (where a free oscillator would be now) / SYNCED
        void start (const OscBankSettings& s, const std::array<float, 4>& partHz, double absTimeSec, uint32_t hitCount)
        {
            for (int i = 0; i < numOsc; ++i)
            {
                auto& u = unit[(size_t) i];
                const auto& o = s.o[(size_t) i];
                u = OscUnit();
                u.nz.reset (o.seed * 2246822519u + hitCount * 3266489917u + 1u);
                u.rng.seed (o.seed + hitCount * 101u);
                const float hz = baseHz (o, partHz);
                float p0 = o.phase01;
                if (o.phaseMode == PhRandom) p0 += u.rng.next01() * o.phaseRand;
                else if (o.phaseMode == PhFree) p0 += (float) std::fmod ((double) hz * absTimeSec, 1.0);
                else if (o.phaseMode == PhSynced)                       // PHASE ALIGN: start where the source oscillator starts (+ own offset)
                {
                    const int src = juce::jlimit (0, numOsc - 1, o.phaseSrc);
                    p0 += src < i ? unit[(size_t) src].ph[0] : s.o[(size_t) src].phase01;
                }
                Rng vr; vr.seed (o.seed * 31u + 7u + (o.uniPhase == UPhRandom ? hitCount * 13u : 0u));
                for (int k = 0; k < maxUnison; ++k)
                {
                    float pk = p0;
                    if (k > 0 && o.uniPhase == UPhRandom) pk += vr.next01();
                    else if (k > 0 && o.uniPhase == UPhFree) pk += (float) std::fmod ((double) hz * o.uRatio[(size_t) k] * absTimeSec, 1.0);
                    u.ph[(size_t) k] = pk - std::floor (pk);
                }
                u.wtRandNow = u.wtRandTarget = o.wtStart + (o.wtEnd - o.wtStart) * u.rng.next01();
            }
        }

        static float baseHz (const OscSettings& o, const std::array<float, 4>& partHz)
        {
            return (o.track == TrackFixed ? o.fixedHz : partHz[(size_t) juce::jlimit (0, 3, o.track)]) * o.mul;
        }

        // one sample: adds into dest[4] (mono part inputs) and tailLR (stereo tail input)
        inline void tick (const OscBankSettings& s, const OscTables& tb, const std::array<float, 4>& partHz, float ms, float inv,
                          std::array<float, 4>& dest, float& tailL, float& tailR)
        {
            for (int i = 0; i < numOsc; ++i)
            {
                const auto& o = s.o[(size_t) i];
                auto& u = unit[(size_t) i];
                u.wrapped = false;
                if (! o.on) { u.out = 0.f; continue; }
                const float hz = baseHz (o, partHz);
                // ---- FM: the other oscillators' last outputs (+ smoothed self feedback) move the FREQUENCY
                float fmIn = 0.f;
                for (int a = 0; a < numOsc; ++a) if (a != i && s.fm[a][i] > 0.f) fmIn += s.fm[a][i] * unit[(size_t) a].out;
                if (o.fmFb > 0.f) { u.fbState = 0.5f * (u.out + u.outPrev); fmIn += o.fmFb * 0.7f * u.fbState; }
                const float modEnv = std::exp (-ms / juce::jmax (1.f, o.decayMs));
                if (o.fmEnv > 0.f) fmIn *= (1.f - o.fmEnv) + o.fmEnv * modEnv;
                const float fInst = hz * (1.f + 4.f * fmIn);                    // through-zero: the phase may run backwards (no DC shift)
                // ---- SYNC: the master wrapped this sample (masters with a lower index this sample, others last sample)
                bool doSync = false; float frac = 0.f;
                if (o.syncSrc > 0 && o.syncSrc - 1 != i)
                {
                    const auto& m = unit[(size_t) (o.syncSrc - 1)];
                    if (m.wrapped) { doSync = true; frac = m.wrapFrac; }
                }
                const float slaveMul = o.syncSrc > 0 ? o.syncRatio : 1.f;
                const Table* table = tb.t[(size_t) i];
                const bool tableWave = (o.wave == WCustom || o.wave == WWavetable || o.wave == WHarmonic) && table != nullptr;
                float wtPos = o.wtPos;
                if (tableWave && o.wave == WWavetable) wtPos = scanPos (o, u, ms);
                const int n = o.wave == WNoise ? 1 : juce::jlimit (1, maxUnison, o.uni);
                float yL = 0.f, yR = 0.f, jump = 0.f;
                for (int k = 0; k < n; ++k)
                {
                    const float dt = juce::jlimit (-0.45f, 0.45f, fInst * o.uRatio[(size_t) k] * slaveMul * inv);
                    const float adt = std::abs (dt);
                    float& p = u.ph[(size_t) k];
                    float v;
                    if (o.wave == WNoise) v = u.nz.tick (o.noise, fInst, inv);
                    else
                    {
                        if (doSync && (o.syncMode == 0 || p > 0.5f))           // HARD: always reset; SOFT: only in the second half
                        {
                            const float before = shape (o, table, wtPos, p, adt);
                            const float np = frac * adt;                     // where the slave would be since the master's wrap
                            const float after = shape (o, table, wtPos, np, adt);
                            jump += (after - before) * o.uGain[(size_t) k];
                            p = np;
                        }
                        v = shape (o, table, wtPos, p, adt);
                        p += dt;
                        if (p >= 1.f) { p -= 1.f; if (k == 0) { u.wrapped = true; u.wrapFrac = adt > 0.f ? p / adt : 0.f; } }
                        else if (p < 0.f) { p += 1.f; if (k == 0) { u.wrapped = true; u.wrapFrac = adt > 0.f ? (1.f - p) / adt : 0.f; } }
                    }
                    const float g = o.uGain[(size_t) k];
                    yL += v * g * o.uL[(size_t) k]; yR += v * g * o.uR[(size_t) k];
                }
                // polyBLEP on the sync reset: half of the step before (into the delayed sample), half after
                float y = 0.5f * (yL + yR);
                if (jump != 0.f)
                {
                    const float d = frac;                                       // 0..1 into the current sample
                    u.pending += jump * 0.5f * (1.f - d) * (1.f - d);           // the sample before smooths towards the new value
                    const float corrNow = -jump * 0.5f * d * d;
                    yL += corrNow; yR += corrNow; y += corrNow;
                }
                // ---- RING: multiplication by another oscillator / an internal sine (not a level change: the spectrum moves)
                if (o.ringSrc > 0)
                {
                    float m;
                    if (o.ringSrc == 5) { m = std::sin (twoPi * u.ringPh); u.ringPh += juce::jlimit (0.f, 0.45f, o.ringHz * inv); if (u.ringPh >= 1.f) u.ringPh -= 1.f; }
                    else { const int src = o.ringSrc - 1; m = src == i ? u.out : unit[(size_t) src].out; }
                    float amt = o.ringAmt;
                    if (o.ringEnv > 0.f) amt *= (1.f - o.ringEnv) + o.ringEnv * modEnv;
                    const float mod = 1.f - amt + amt * m;
                    const float w = o.ringMix;
                    yL = yL * (1.f - w) + yL * mod * w; yR = yR * (1.f - w) + yR * mod * w; y = 0.5f * (yL + yR);
                }
                if (! std::isfinite (y)) { yL = yR = y = 0.f; }
                u.outPrev = u.out; u.out = y;
                // one-sample delay so the sync correction can still reach the previous sample
                const float outL = u.L + u.pending, outR = u.R + u.pending;
                u.pending = 0.f;
                u.L = yL; u.R = yR;
                if (! o.audible) continue;                                      // MUTE / not SOLO: still a modulation source
                const float gl = outL * o.gain * o.panL, gr = outR * o.gain * o.panR, mono = 0.5f * (gl + gr);
                for (int d = 0; d < 4; ++d) dest[(size_t) d] += mono * o.route[(size_t) d];
                tailL += gl * o.route[2]; tailR += gr * o.route[2];
            }
        }
        const OscUnit& unitOf (int i) const { return unit[(size_t) i]; }

        // module 04: a LAYER playing "OSC n" runs its own copy of that oscillator at its own pitch (own phases, unison,
        // table, self feedback and sine ring - the matrix FM / sync to other oscillators belong to the oscillator page)
        inline float tickOne (const OscSettings& o, const OscTables& tb, int i, float hz, float ms, float inv)
        {
            auto& u = unit[(size_t) i];
            float fmIn = 0.f;
            if (o.fmFb > 0.f) { u.fbState = 0.5f * (u.out + u.outPrev); fmIn = o.fmFb * 0.7f * u.fbState; }
            const float modEnv = std::exp (-ms / juce::jmax (1.f, o.decayMs));
            if (o.fmEnv > 0.f) fmIn *= (1.f - o.fmEnv) + o.fmEnv * modEnv;
            const float fInst = hz * (1.f + 4.f * fmIn);
            const Table* table = tb.t[(size_t) i];
            float wtPos = o.wtPos;
            if (o.wave == WWavetable) wtPos = scanPos (o, u, ms);
            const int n = o.wave == WNoise ? 1 : juce::jlimit (1, maxUnison, o.uni);
            float y = 0.f;
            for (int k = 0; k < n; ++k)
            {
                const float dt = juce::jlimit (-0.45f, 0.45f, fInst * o.uRatio[(size_t) k] * inv);
                float& p = u.ph[(size_t) k];
                float v;
                if (o.wave == WNoise) v = u.nz.tick (o.noise, fInst, inv);
                else { v = shape (o, table, wtPos, p, std::abs (dt)); p += dt; if (p >= 1.f) p -= 1.f; else if (p < 0.f) p += 1.f; }
                y += v * o.uGain[(size_t) k];
            }
            if (o.ringSrc == 5)
            {
                const float m = std::sin (twoPi * u.ringPh); u.ringPh += juce::jlimit (0.f, 0.45f, o.ringHz * inv); if (u.ringPh >= 1.f) u.ringPh -= 1.f;
                float amt = o.ringAmt; if (o.ringEnv > 0.f) amt *= (1.f - o.ringEnv) + o.ringEnv * modEnv;
                y = y * (1.f - o.ringMix) + y * (1.f - amt + amt * m) * o.ringMix;
            }
            if (! std::isfinite (y)) y = 0.f;
            u.outPrev = u.out; u.out = y;
            return y * (o.gain < 0.f ? -1.f : 1.f);
        }

    private:
        static inline float shape (const OscSettings& o, const Table* table, float wtPos, float p, float dt)
        {
            switch (o.wave)
            {
                case WSine:     return std::sin (twoPi * p);
                case WTriangle: return 1.f - 4.f * std::abs (p - 0.5f);
                case WSaw:      return 2.f * p - 1.f - polyBlep (p, dt);
                case WSquare:   { float q = p + 0.5f; if (q >= 1.f) q -= 1.f; return (p < 0.5f ? 1.f : -1.f) + polyBlep (p, dt) - polyBlep (q, dt); }
                case WPulse:
                {
                    const float w = juce::jlimit (0.01f, 0.99f, o.pw);
                    float q = p + 1.f - w; if (q >= 1.f) q -= 1.f;
                    return (p < w ? 1.f : -1.f) + polyBlep (p, dt) - polyBlep (q, dt) - (2.f * w - 1.f);
                }
                case WCustom: case WWavetable: case WHarmonic:
                    return table != nullptr ? table->read (p, wtPos, o.wtMorph, Table::mipFor (dt)) : std::sin (twoPi * p);
                default: return 0.f;
            }
        }
        // WAVETABLE position over time (the scan modes)
        static inline float scanPos (const OscSettings& o, OscUnit& u, float ms)
        {
            const float a = o.wtStart, b = o.wtEnd, T = juce::jmax (5.f, o.wtRateMs);
            switch (o.wtMode)
            {
                case WtLinear:   return a + (b - a) * juce::jmin (1.f, ms / T);
                case WtEnvelope: return b + (a - b) * std::exp (-ms / T * 4.f);
                case WtLfo:      return a + (b - a) * (0.5f - 0.5f * std::cos (twoPi * ms / T));
                case WtRandom:
                {
                    if (ms >= u.wtRandMs) { u.wtRandNow = u.wtRandTarget; u.wtRandTarget = a + (b - a) * u.rng.next01(); u.wtRandMs = ms + T; }
                    const float x = 1.f - juce::jlimit (0.f, 1.f, (u.wtRandMs - ms) / T);
                    return u.wtRandNow + (u.wtRandTarget - u.wtRandNow) * x;
                }
                default: return o.wtPos;
            }
        }
        std::array<OscUnit, maxOsc> unit {};
    };
}
