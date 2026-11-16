#pragma once
// =====================================================================
//  4GE - TAIL TYPE EXPERIMENTAL (owner's spec 2026-11-16, FourGE/TAIL_RAW_EXP.md): the sound laboratory - metallic tails, unstable
//  textures, FM bass tails, formants, unusual pitch movement, digital / glitchy tails. Its own engine (rule 0), on the shared skeleton:
//    01 SOURCE    OSCILLATOR SINE / TRI / SAW / SQUARE / CUSTOM WAVETABLE (a morph table) + a REAL FM ENGINE (a sine carrier phase-
//                 modulated by a sine modulator: AMOUNT = index, RATIO) + NOISE ENGINE WHITE / PINK / DIGITAL + the RING MOD SOURCE
//    02 PITCH     curves LINEAR DOWN / UP, EXPONENTIAL DOWN / UP, MULTI-STEP, SINE MODULATION, RANDOM, CUSTOM; START / END / RANGE /
//                 TIME / CURVE / MOD RATE / MOD DEPTH
//    04 MATRIX    8 FX SLOTS in a chain whose ORDER is the slot order (the user re-orders by moving a slot). 10 processors: FORMANT,
//                 FREQUENCY SHIFTER, RING MOD, PITCH SHIFTER, WAVESHAPER, BITCRUSHER, PHASER, COMB, STRETCH, RESONATOR.
//                 Every slot: TYPE (OFF = bypass), MIX, AMOUNT, a characteristic PARAM, a MOD SOURCE + DEPTH (onto AMOUNT).
//    06 MOD       ENVELOPE, LFO 1, LFO 2, RANDOM (deterministic with FIXED SEED: the same note = the same render), KICK POSITION,
//                 MANUAL CURVE (3 points)
//    07 STEREO    LOW (under SUB HZ) always MONO, MID width = the user's, HIGH = stereo FX (the chain runs L / R with mirrored
//                 shifts / phases); a phase modulation can never lose the fundamental (the lows are rebuilt mono)
//    08 SUB       the lows replaced by a clean sine on the pitch (mono), blend
//    09 OUTPUT    soft clip + the tail shape over the KICK LENGTH
//  Macros (the shared TAIL set, EXPERIMENTAL mapping): DRIVE = nonlinearity, TONE = colour / spectrum, MOVEMENT = modulation depth,
//  RESONANCE = formant / comb / resonator, DENSITY = texture complexity (FM + noise), SPACE = spatial FX, CHAOS = random / FM / glitch,
//  WIDTH = creative stereo. DSP only; no allocation in process() (buffers made in prepare()).
// =====================================================================
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <array>
#include <vector>
#include "Biquad.h"

namespace fge::dsp::exper
{
    enum Fx { Off = 0, Formant, FreqShift, RingMod, PitchShift, Waveshaper, Bitcrusher, Phaser, Comb, Stretch, Resonator, NumFx };
    enum ModSrc { MNone = 0, MEnv, MLfo1, MLfo2, MRandom, MKickPos, MCurve };
    constexpr int numSlots = 8;

    struct Slot { int type = Off; float mix = 1.f, amount = 0.5f, param = 0.5f; int modSrc = MNone; float modDepth = 0.f; };

    struct Settings
    {
        bool on = false;
        // 01 source
        int osc = 2; float wtPos = 0.f, oscLevel = 1.f;
        float fmLevel = 0.f, fmAmount = 2.f, fmRatio = 2.f;
        int noise = 0; float noiseLevel = 0.f;
        float ringRatio = 1.5f;
        // 02 pitch
        int pitchCurve = 2; float startSt = 24.f, endSt = 0.f, midSt = 7.f, range = 1.f, pitchMs = 120.f, pitchCurveAmt = 0.5f, pmRate = 6.f, pmDepth = 0.f, rootSt = 0.f;
        // 04 matrix
        std::array<Slot, numSlots> slot {};
        // 06 modulation
        float lfo1Rate = 4.f, lfo2Rate = 9.f, rndRate = 12.f, curve0 = 1.f, curve1 = 0.2f, curve2 = 0.6f; bool fixedSeed = true; int seedNote = 0;
        float fmMod = 0.f, fsMod = 0.f;                          // independent modulation of the FM amount / every freq shifter (LFO 2 / RANDOM)
        // macros (0.5 = neutral)
        float mDrive = 0.5f, mTone = 0.5f, mMove = 0.5f, mReso = 0.5f, mDensity = 0.5f, mSpace = 0.5f, mChaos = 0.5f, mWidth = 0.5f;
        // 07 / 08 / 09
        float sub = 0.8f, subHz = 120.f, width = 0.5f, drive = 0.3f;
        float lengthMs = 400.f, attackMs = 2.f, body = 0.6f, decay = 0.5f, fade = 0.5f;
    };

    // ---- the pitch curve in semitones over u = 0..1 (pure: the tests call it)
    inline float pitchSt (const Settings& s, float u, float ms, float rnd)
    {
        u = std::clamp (u, 0.f, 1.f);
        const float a = s.startSt * s.range, b = s.endSt * s.range, k = 0.5f + 4.f * std::clamp (s.pitchCurveAmt, 0.f, 1.f);
        float st = 0.f;
        switch (s.pitchCurve)
        {
            case 0: st = a + (b - a) * u; break;                                              // LINEAR DOWN (start -> end)
            case 1: st = b + (a - b) * u; break;                                              // LINEAR UP (end -> start)
            case 2: st = b + (a - b) * std::pow (1.f - u, k); break;                          // EXPONENTIAL DOWN
            case 3: st = a + (b - a) * std::pow (1.f - u, k); break;                          // EXPONENTIAL UP
            case 4: st = a + (b - a) * std::floor (u * 4.f) / 3.f; break;                     // MULTI-STEP (4 steps)
            case 5: st = b + (a - b) * std::pow (1.f - u, k); break;                          // SINE MODULATION (on an exp drop, + below)
            case 6: st = b + (a - b) * std::pow (1.f - u, k) + rnd * 0.5f * std::abs (a - b); break;   // RANDOM (stepped)
            default: st = u < 0.5f ? a + (s.midSt * s.range - a) * (u * 2.f) : s.midSt * s.range + (b - s.midSt * s.range) * (u * 2.f - 1.f); break;   // CUSTOM
        }
        if (s.pitchCurve == 5 || s.pmDepth > 0.f) st += s.pmDepth * std::sin (6.2831853f * s.pmRate * ms * 0.001f);
        return st;
    }

    class Unit
    {
    public:
        void prepare (double rate)
        {
            fs = rate;
            for (int c = 0; c < 2; ++c) for (int k = 0; k < numSlots; ++k)
            {
                combBuf[c][k].assign ((size_t) (rate / 20.0) + 8, 0.f);
                psBuf[c][k].assign ((size_t) (rate * 0.06) + 8, 0.f);
                stBuf[c][k].assign ((size_t) (rate * 0.12) + 8, 0.f);
            }
            reset();
        }
        void reset() { resetCore (nullptr); }
        // per kick: the buffers of the slots in use only (no big memset on the audio thread for empty slots)
        void resetFor (const Settings& s) { resetCore (&s); }
        void resetCore (const Settings* s)
        {
            po = pf = pm = pr = 0.0; blk = 0; lfo1 = lfo2 = 0.0; pk0 = pk1 = pk2 = 0.f; holdN = 0; hold = 0.f; rndNow = 0.f; rndT = 0.f; nowMs = 0.f;
            seed = 0x9e3779b9u;
            for (int c = 0; c < 2; ++c)
            {
                for (int k = 0; k < numSlots; ++k)
                {
                    auto& z = st[c][k]; z = {};
                    for (auto* b : { &z.f1, &z.f2, &z.f3, &z.rs }) b->clear();
                    if (s != nullptr && s->slot[(size_t) k].type == Off) continue;
                    std::fill (combBuf[c][k].begin(), combBuf[c][k].end(), 0.f);
                    std::fill (psBuf[c][k].begin(), psBuf[c][k].end(), 0.f);
                    std::fill (stBuf[c][k].begin(), stBuf[c][k].end(), 0.f);
                }
                for (auto& h : hil[c]) h = {};
                for (auto* b : { &lowA[c], &lowB[c], &hiA[c], &hiB[c], &midA[c], &midB[c], &tilt[c] }) b->clear();
                for (auto* b : { &sc1, &sc2, &sc3, &sc4, &sc5, &sc6 }) b->clear();
            }
        }

        void process (const Settings& s, float ms, float noteHz, float& outL, float& outR)
        {
            if (ms < nowMs) {}                                                    // (reset() per kick; ms only grows within one)
            nowMs = ms;
            if (blk-- <= 0) { blk = 32; cook (s, ms, noteHz); }
            // ---- 01 SOURCE: oscillator (+ wavetable morph), REAL FM (phase modulation), noise
            const double inc = hz / fs;
            float x = s.oscLevel * osc (s.osc, po, s.wtPos);
            if (s.fmLevel > 0.f)
            {
                const double mod = std::sin (6.283185307 * pf);
                x += s.fmLevel * (float) std::sin (6.283185307 * po + fmIndex * mod);       // carrier phase-modulated by the modulator
            }
            if (s.noiseLevel > 0.f) x += s.noiseLevel * noiseOf (s.noise);
            const float sine = (float) std::sin (6.283185307 * po);
            const float ring = (float) std::sin (6.283185307 * pr);
            po += inc; if (po >= 1.0) po -= 1.0;
            pf += inc * fmRatioNow; if (pf >= 1.0) pf -= 1.0;
            pr += inc * s.ringRatio; if (pr >= 1.0) pr -= 1.0;
            lfo1 += s.lfo1Rate / fs; if (lfo1 >= 1.0) lfo1 -= 1.0;
            lfo2 += s.lfo2Rate / fs; if (lfo2 >= 1.0) lfo2 -= 1.0;
            x *= preGain;
            // ---- 04 MATRIX: the 8 slots in their order, L / R (the right side mirrors shifts / phases = the stereo FX)
            float ch[2] { x, x };
            for (int k = 0; k < numSlots; ++k)
            {
                const auto& sl = s.slot[(size_t) k];
                if (sl.type == Off || sl.mix <= 0.f) continue;
                for (int c = 0; c < 2; ++c) ch[c] = runFx (sl, k, c, ch[c], ring);
            }
            // ---- 07 / 08: LOW mono rebuilt (clean sine blend), MID user width, HIGH stereo FX
            float out[2];
            // M / S: the side is high-passed at 48 dB / oct under SUB HZ first (nothing can widen the lows), then MID width / HIGH width
            float mid = 0.5f * (ch[0] + ch[1]), side = 0.5f * (ch[0] - ch[1]);
            side = sc4.tick (sc3.tick (sc2.tick (sc1.tick (side))));
            const float sideHi = hiB[0].tick (hiA[0].tick (side));
            side = (side - sideHi) * midWidth + sideHi * hiWidth;
            const float lowM = lowB[0].tick (lowA[0].tick (mid));                        // the mono lows <-> a clean sine on the pitch
            mid += (subSine * sine - lowM) * subBlend;
            // ---- 09 OUTPUT: soft clip on MID and SIDE apart (a clip on L / R would intermodulate the side with the sub = lows on the side)
            const float e = shapeEnv (s, ms), norm = 1.f / std::tanh (outDrive);
            mid = tilt[0].tick (mid); side = tilt[1].tick (side);
            mid = std::tanh (mid * outDrive) * norm; side = sc6.tick (sc5.tick (std::tanh (side * outDrive) * norm));   // (and the side high-passed once more after its clip)
            out[0] = (mid + side) * e * 0.9f; out[1] = (mid - side) * e * 0.9f;
            outL = out[0]; outR = out[1];
        }

        // (tests)
        float lastPitchHz() const { return (float) hz; }

    private:
        struct SlotState { bq::Biquad f1, f2, f3, rs; float ap[6] {}, fb = 0.f, hold = 0.f; int holdN = 0, combIdx = 0, psIdx = 0, stIdx = 0; double psPh = 0.0, stRead = 0.0; };
        struct Hilbert { float x1[4] {}, y1[4] {}, x2[4] {}, y2[4] {}; double ph = 0.0; };

        float osc (int type, double p, float wt) const
        {
            const float sn = (float) std::sin (6.283185307 * p), saw = (float) (2.0 * p - 1.0), sq = p < 0.5 ? 0.8f : -0.8f, tri = (float) (4.0 * std::abs (p - 0.5) - 1.0);
            switch (type)
            {
                case 0: return sn;
                case 1: return tri;
                case 3: return sq;
                case 4:                                                                   // CUSTOM WAVETABLE: sine -> tri -> saw -> square -> pulse
                {
                    const float pulse = p < 0.15 ? 0.9f : -0.25f;
                    const float tbl[5] { sn, tri, saw, sq, pulse };
                    const float w = std::clamp (wt, 0.f, 1.f) * 4.f; const int i = std::min (3, (int) w); const float f = w - (float) i;
                    return tbl[i] + (tbl[i + 1] - tbl[i]) * f;
                }
                default: return saw;
            }
        }
        float noiseOf (int type)
        {
            float n = rnd();
            if (type == 1) { pk0 = 0.997f * pk0 + 0.029591f * n; pk1 = 0.985f * pk1 + 0.032534f * n; pk2 = 0.950f * pk2 + 0.048056f * n; return 4.f * (pk0 + pk1 + pk2 + 0.0187f * n); }
            if (type == 2) { if (holdN-- <= 0) { holdN = 24; hold = std::round (n * 3.f) / 3.f; } return hold; }   // DIGITAL: stepped, crushed
            return n;
        }
        float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f); }
        float modValue (int src) const
        {
            switch (src)
            {
                case MEnv: return envNow;
                case MLfo1: return (float) std::sin (6.283185307 * lfo1);
                case MLfo2: return (float) (4.0 * std::abs (lfo2 - 0.5) - 1.0);
                case MRandom: return rndNow;
                case MKickPos: return posNow;
                case MCurve: return curveNow;
                default: return 0.f;
            }
        }
        float runFx (const Slot& sl, int k, int c, float x, float ring)
        {
            auto& z = st[c][(size_t) k];
            const float amt = std::clamp (sl.amount + sl.modDepth * modDepthMul * modValue (sl.modSrc), 0.f, 1.f);
            const float side = c == 0 ? -1.f : 1.f;                                        // the R side mirrors (stereo FX)
            float y = x;
            switch (sl.type)
            {
                case Formant:                                                             // 3 formant band-passes, AMOUNT = the vowel (A E I O U)
                {
                    static const float F[5][3] { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
                    const float v = amt * 4.f; const int i = std::min (3, (int) v); const float t = v - (float) i;
                    const float sh = std::exp2 ((sl.param - 0.5f) * 1.5f);                // PARAM = formant shift
                    if (z.holdN-- <= 0)
                    {
                        z.holdN = 32; const double q = 6.0 + 10.0 * resoMul;
                        z.f1.bp (fs, std::clamp ((double) (F[i][0] + (F[i + 1][0] - F[i][0]) * t) * sh, 60.0, fs * 0.4), q);
                        z.f2.bp (fs, std::clamp ((double) (F[i][1] + (F[i + 1][1] - F[i][1]) * t) * sh, 60.0, fs * 0.4), q);
                        z.f3.bp (fs, std::clamp ((double) (F[i][2] + (F[i + 1][2] - F[i][2]) * t) * sh, 60.0, fs * 0.4), q);
                    }
                    y = 2.5f * (z.f1.tick (x) + 0.7f * z.f2.tick (x) + 0.4f * z.f3.tick (x));
                    break;
                }
                case FreqShift:                                                           // a Hilbert pair -> single-sideband shift (metallic, inharmonic)
                {
                    const float shiftHz = ((amt - 0.5f) * 2.f * 600.f + fsModNow) * (1.f + side * 0.15f * spaceAmt);
                    float re, im; hilbert (hil[c][(size_t) k], x, re, im);
                    auto& h = hil[c][(size_t) k];
                    h.ph += shiftHz / fs; h.ph -= std::floor (h.ph);
                    y = re * (float) std::cos (6.283185307 * h.ph) - im * (float) std::sin (6.283185307 * h.ph);
                    break;
                }
                case RingMod: y = x * (ring * (0.5f + 0.5f * amt) + (1.f - amt) * 0.5f); break;   // bell-like, inharmonic (the RING SOURCE)
                case PitchShift:                                                          // a 2-tap delay-line shifter, AMOUNT = -12..+12 st
                {
                    auto& b = psBuf[c][(size_t) k]; const int n = (int) b.size(); const double win = fs * 0.03;
                    b[(size_t) z.psIdx] = x;
                    const double ratio = std::exp2 ((amt - 0.5) * 24.0 / 12.0);
                    z.psPh += (1.0 - ratio) / win; z.psPh -= std::floor (z.psPh);
                    auto tap = [&] (double ph) { double d = ph * win + 2.0; double r = z.psIdx - d; while (r < 0) r += n; const int i0 = (int) r; const float f = (float) (r - i0);
                                                 return b[(size_t) i0] + (b[(size_t) ((i0 + 1) % n)] - b[(size_t) i0]) * f; };
                    const double ph2 = z.psPh + 0.5 - std::floor (z.psPh + 0.5);
                    const float w1 = (float) std::sin (3.14159265 * z.psPh), w2 = (float) std::sin (3.14159265 * ph2);
                    y = tap (z.psPh) * w1 * w1 + tap (ph2) * w2 * w2;
                    z.psIdx = (z.psIdx + 1) % n;
                    break;
                }
                case Waveshaper:                                                          // nonlinear shaping, PARAM = soft -> fold
                {
                    const float g = 1.f + 20.f * amt * driveMul;
                    const float t = std::tanh (x * g), f = std::sin (1.5707963f * std::clamp (x * g, -4.f, 4.f));
                    y = t + (f - t) * sl.param;
                    break;
                }
                case Bitcrusher:                                                          // AMOUNT = bits (16 -> 2), PARAM = sample-rate reduction
                {
                    const int hn = 1 + (int) (sl.param * sl.param * 60.f * (1.f + chaosAmt));
                    if (z.holdN-- <= 0) { z.holdN = hn; const float q = std::exp2 (2.f + 14.f * (1.f - amt)); z.hold = std::round (x * q) / q; }
                    y = z.hold;
                    break;
                }
                case Phaser:                                                              // 6 allpasses swept by its own LFO (R offset = stereo phaser)
                {
                    const float lf = 0.5f + 0.5f * (float) std::sin (6.283185307 * (lfo1 + (c ? 0.25 * spaceAmt : 0.0)));
                    const double f = 200.0 * std::pow (30.0, sl.param * 0.6 + 0.4 * lf);
                    const double t = std::tan (3.14159265 * std::min (f, fs * 0.45) / fs); const float a = (float) ((t - 1.0) / (t + 1.0));
                    float v = x + z.fb * 0.85f * amt;
                    for (auto& s1 : z.ap) { const float o = a * v + s1; s1 = v - a * o; v = o; }
                    z.fb = v; y = 0.5f * (x + v);
                    break;
                }
                case Comb:                                                                // metallic resonances on the pitch (PARAM = ratio), AMOUNT = feedback
                {
                    auto& b = combBuf[c][(size_t) k]; const int n = (int) b.size();
                    const int len = std::clamp ((int) (fs / std::max (20.0, hz * (0.5 + 3.5 * sl.param))), 1, n - 1);
                    int r = z.combIdx - len; if (r < 0) r += n;
                    const float fbk = std::clamp (amt * (0.7f + 0.28f * resoMul), 0.f, 0.97f);
                    y = x + fbk * b[(size_t) r];
                    b[(size_t) z.combIdx] = y; z.combIdx = (z.combIdx + 1) % n;
                    y *= 1.f - 0.6f * fbk;
                    break;
                }
                case Stretch:                                                             // texture stretching: read the recent past slower (AMOUNT = stretch)
                {
                    auto& b = stBuf[c][(size_t) k]; const int n = (int) b.size();
                    b[(size_t) z.stIdx] = x;
                    const double speed = 1.0 - 0.9 * amt;
                    z.stRead += speed; if (z.stRead >= n) z.stRead -= n;
                    double lag = z.stIdx - z.stRead; if (lag < 0) lag += n;
                    if (lag > n * 0.9) z.stRead = z.stIdx - 2.0;                              // (catch up before the buffer wraps)
                    double r = z.stRead; if (r < 0) r += n;
                    const int i0 = (int) r % n; const float f = (float) (r - std::floor (r));
                    y = b[(size_t) i0] + (b[(size_t) ((i0 + 1) % n)] - b[(size_t) i0]) * f;
                    z.stIdx = (z.stIdx + 1) % n;
                    break;
                }
                case Resonator:                                                           // a narrow ringing emphasis on (pitch x PARAM)
                {
                    if (z.holdN-- <= 0) { z.holdN = 32; z.rs.bp (fs, std::clamp (hz * (1.0 + 7.0 * sl.param), 40.0, fs * 0.4), 4.0 + 40.0 * amt * resoMul); }
                    y = x + (2.f + 6.f * amt) * z.rs.tick (x);
                    break;
                }
                default: break;
            }
            return x + (y - x) * std::clamp (sl.mix, 0.f, 1.f);
        }
        // a 4-stage allpass Hilbert pair (Olli Niemitalo's coefficients, valid at any rate for the audio band)
        void hilbert (Hilbert& h, float x, float& re, float& im) const
        {
            static const float a1[4] { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            static const float a2[4] { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            float u = x, v = x;
            for (int i = 0; i < 4; ++i)
            {
                const float c1 = a1[i] * a1[i], c2 = a2[i] * a2[i];
                const float o1 = c1 * (u + h.y1[i]) - h.x1[i]; h.x1[i] = u; h.y1[i] = o1; u = o1;
                const float o2 = c2 * (v + h.y2[i]) - h.x2[i]; h.x2[i] = v; h.y2[i] = o2; v = o2;
            }
            re = u; im = v;
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
            const float L = std::max (20.f, s.lengthMs);
            posNow = std::clamp (ms / L, 0.f, 1.f);
            envNow = 1.f - posNow;
            curveNow = posNow < 0.5f ? s.curve0 + (s.curve1 - s.curve0) * posNow * 2.f : s.curve1 + (s.curve2 - s.curve1) * (posNow * 2.f - 1.f);
            // RANDOM: stepped, deterministic (FIXED SEED: the seed is the note, set at reset by the engine)
            rndT += 32.f / (float) fs * 1000.f;
            const float chaos = std::clamp ((s.mChaos - 0.5f) * 2.f, 0.f, 1.f);
            chaosAmt = chaos;
            if (rndT >= 1000.f / std::max (0.5f, s.rndRate * (1.f + 2.f * chaos))) { rndT = 0.f; rndNow = rnd(); }
            // macros
            modDepthMul = std::clamp (s.mMove * 2.f, 0.f, 2.f);                          // MOVEMENT: every modulation depth
            resoMul = std::clamp (s.mReso * 2.f, 0.f, 2.f);                               // RESONANCE: formant / comb / resonator
            driveMul = std::clamp (s.mDrive * 2.f, 0.f, 2.f);                             // DRIVE: the nonlinearity
            spaceAmt = std::clamp (s.mSpace * 2.f, 0.f, 2.f);                             // SPACE: the stereo FX spread
            const float dens = std::clamp (s.mDensity * 2.f, 0.f, 2.f);                   // DENSITY: FM + noise
            // 02 PITCH
            const float u = std::clamp (ms / std::max (5.f, s.pitchMs), 0.f, 1.f);
            const double root = std::max (10.0, (double) noteHz * std::exp2 (s.rootSt / 12.0));
            hz = std::clamp (root * std::exp2 (pitchSt (s, u, ms, rndNow) / 12.0), 5.0, fs * 0.3);
            // FM (+ its independent modulation by LFO 2, + CHAOS jitter)
            fmIndex = s.fmAmount * dens * (1.f + s.fmMod * modValue (MLfo2) * modDepthMul) * (1.f + 0.5f * chaos * rndNow);
            fmRatioNow = s.fmRatio * (1.0 + 0.03 * chaos * rndNow);
            fsModNow = s.fsMod * 300.f * modValue (MRandom) * modDepthMul;
            preGain = 1.f + 0.6f * std::max (0.f, dens - 1.f);
            // STEREO + SUB + OUT
            for (int c = 0; c < 2; ++c)
            {
                lowA[c].lp (fs, s.subHz, 0.7071); lowB[c].lp (fs, s.subHz, 0.7071);
                if (c == 0) { sc1.hp (fs, s.subHz, 0.5412); sc2.hp (fs, s.subHz, 1.3066); sc3.hp (fs, s.subHz, 0.5412); sc4.hp (fs, s.subHz, 1.3066); sc5.hp (fs, s.subHz, 0.7071); sc6.hp (fs, s.subHz, 0.7071); }
                hiA[c].hp (fs, 2500.0, 0.7071); hiB[c].hp (fs, 2500.0, 0.7071);
                tilt[c].shelf (fs, 1500.0, (s.mTone - 0.5f) * 14.f, true);               // TONE: the colour / spectrum
            }
            const float wd = std::clamp (s.mWidth * 2.f, 0.f, 2.f);
            midWidth = std::clamp (s.width * wd, 0.f, 1.5f);
            hiWidth = std::clamp ((0.6f + s.width) * wd * (0.5f + 0.5f * spaceAmt), 0.f, 2.f);
            subBlend = std::clamp (s.sub, 0.f, 1.f); subSine = 0.8f * std::max (1.f, s.sub);
            outDrive = 1.f + 6.f * s.drive * driveMul;
        }

        double fs = 48000.0, po = 0.0, pf = 0.0, pm = 0.0, pr = 0.0, hz = 50.0, lfo1 = 0.0, lfo2 = 0.0, fmRatioNow = 2.0;
        int blk = 0, holdN = 0;
        uint32_t seed = 0x9e3779b9u;
        float hold = 0.f, pk0 = 0.f, pk1 = 0.f, pk2 = 0.f, rndNow = 0.f, rndT = 0.f, nowMs = 0.f;
        float posNow = 0.f, envNow = 1.f, curveNow = 1.f, modDepthMul = 1.f, resoMul = 1.f, driveMul = 1.f, spaceAmt = 1.f, chaosAmt = 0.f;
        float fmIndex = 0.f, fsModNow = 0.f, preGain = 1.f, midWidth = 0.5f, hiWidth = 1.f, subBlend = 0.8f, subSine = 0.8f, outDrive = 1.f;
        SlotState st[2][numSlots] {};
        std::array<Hilbert, numSlots> hil[2] {};
        std::vector<float> combBuf[2][numSlots], psBuf[2][numSlots], stBuf[2][numSlots];
        bq::Biquad lowA[2], lowB[2], hiA[2], hiB[2], midA[2], midB[2], tilt[2], sc1, sc2, sc3, sc4, sc5, sc6;
    public:
        void seedFor (int note, bool fixed, uint32_t hitCount) { seed = fixed ? 0x9e3779b9u ^ (uint32_t) (note * 2654435761u) : 0x9e3779b9u ^ (hitCount * 40503u); }
    };
}
