#pragma once
#include <cstring>
// =====================================================================
//  4GE module 08 - DYNAMICS (pure DSP: no GUI, no files, no allocation after construction). Every processor sits at a place
//  (GLOBAL / PUNCH / BODY / TAIL / SUB / TEXTURE / LAYER 1-8 / SCREECH = at the engine rate, BEFORE / AFTER that place's
//  distortion; MASTER OUTPUT = at the host rate after the oversampling, the only place where LOOKAHEAD works):
//   COMPRESSOR (4)  - VCA (feed-forward, dB-domain envelope), FET (feedback detector + gain-cell colour), OPTO (release that
//                     grows with the reduction history), VARI-MU (ratio grows with the level), DIGITAL (linear-domain envelope,
//                     hard computer), UPWARD (lifts below the threshold), PARALLEL (dry / wet levels, no level doubling),
//                     SIDECHAIN (the detector hears only the chosen source); PEAK / RMS / HYBRID / TRUE PEAK detection,
//                     sidechain source (self / a part / texture) with HPF / LPF / gain / mix, auto release, stereo link, lookahead;
//   TRANSIENT (5)   - attack / sustain from two real envelopes (fast / slow), 5 detections, 4 modes using the kick's own time;
//   CLIPPER (2)     - soft / hard / asymmetric / sine / cubic / tanh / custom curve, ceiling, drive, symmetry, softness, extra OS;
//   LIMITER (2)     - gain from a running minimum over the lookahead (no overshoot), true peak (interpolated), 5 strategies,
//                     ceiling never passed (final guard), gain reduction meters;
//   CREST CONTROL   - measures peak / RMS and moves the crest factor toward a target (punch / body / tail zones of the kick);
//   AUTO GAIN       - per processor: windowed in / out level (peak / RMS / K-weighted) -> slow correction (never per sample);
//   LEVEL MATCH     - every processor outputs its input at the processed level (A / B at matched loudness).
// =====================================================================
#include "../Tone/Tone.h"
#include "../Dist/Distortion.h"
#include <array>
#include <cmath>

namespace fge::dsp::dyn
{
    using m06::Ctx;
    constexpr int LMaster = 15, NumLocs = 16, numCmp = 4, numTs = 5, numClp = 2, numLim = 2;
    constexpr int maxLook = 4096;                                        // 20 ms at 192 kHz
    enum CType { VCA = 0, FET, Opto, VariMu, Digital, Upward, Parallel, Sidechain };
    enum Det { DPeak = 0, DRms, DHybrid, DTrue };
    enum ARel { ArManual = 0, ArFast, ArMedium, ArSlow, ArProgram };
    enum TsDet { TdFast = 0, TdSlow, TdRms, TdPeak, TdHybrid };
    enum TsMode { TmPunch = 0, TmSustain, TmBody, TmTail };
    enum Clip { CSoft = 0, CHard, CAsym, CSine, CCubic, CTanh, CCustom };
    enum LimMode { LTransparent = 0, LAggressive, LPunch, LLoud, LTrue };
    enum Kind { KCmp = 0, KTs, KClp, KLim, KCrest };
    inline float coefMs (float ms, double rate) { return ms <= 0.f ? 1.f : 1.f - std::exp (-1.f / (0.001f * ms * (float) rate)); }
    inline float db (float g) { return juce::Decibels::gainToDecibels (g, -120.f); }
    // fast dB <-> gain for the per-sample detectors (error < 0.002 dB; the GUI curves use the exact functions)
    inline float fastDb (float g)
    {
        g = juce::jmax (1.0e-6f, g);
        uint32_t b; std::memcpy (&b, &g, 4);
        const int e = (int) ((b >> 23) & 255u) - 127;
        b = (b & 0x007fffffu) | 0x3f800000u; float m; std::memcpy (&m, &b, 4);       // [1, 2)
        const float t = (m - 1.f) / (m + 1.f), t2 = t * t;
        const float l2 = (float) e + 2.8853900818f * t * (1.f + t2 * (0.33333333f + t2 * (0.2f + t2 * 0.14285714f)));
        return 6.0205999133f * l2;
    }
    inline float fromDb (float d)
    {
        const float x = juce::jlimit (-120.f, 60.f, d) * 0.1660964047f;                 // log2 of the gain
        const float fl = std::floor (x), f = x - fl;
        const float p = 1.f + f * (0.6931471806f + f * (0.2402265070f + f * (0.0555041087f + f * (0.0096181291f + f * 0.0013333558f))));
        const uint32_t eb = (uint32_t) ((int) fl + 127) << 23; float s; std::memcpy (&s, &eb, 4);
        return p * s;
    }

    // ------------------------------------------------------------------ intersample peak (4-point cubic, the samples between)
    struct TruePeak
    {
        float x0 = 0, x1 = 0, x2 = 0, x3 = 0;
        inline float push (float x, int steps)
        {
            x0 = x1; x1 = x2; x2 = x3; x3 = x;
            float m = std::abs (x2);
            for (int k = 1; k < steps; ++k)
            {
                const float t = (float) k / (float) steps;
                const float c0 = x1, c1 = 0.5f * (x2 - x0), c2 = x0 - 2.5f * x1 + 2.f * x2 - 0.5f * x3, c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
                m = juce::jmax (m, std::abs (((c3 * t + c2) * t + c1) * t + c0));
            }
            return juce::jmax (m, std::abs (x1));
        }
    };
    // ------------------------------------------------------------------ AUTO GAIN / LEVEL MATCH: a windowed in / out level (no per-sample normalisation)
    struct Level
    {
        float in = 1.0e-6f, out = 1.0e-6f, g = 1.f; tone::Sec kIn, kOut; tone::Coef kc; bool kSet = false;
        inline void measure (int mode, float x, float y, double rate)
        {
            const float w = 1.f - std::exp (-1.f / (0.05f * (float) rate));    // 50 ms window (a kick is short; still far slower than one cycle)
            if (mode == 1) { in = juce::jmax (std::abs (x), in * (1.f - w)); out = juce::jmax (std::abs (y), out * (1.f - w)); }   // PEAK (decaying hold)
            else
            {
                if (mode == 3) { if (! kSet) { kc = tone::coef (tone::SHighShelf, 1500.0, 0.707, 4.0, rate); kSet = true; } x = (float) kIn.tick (kc, x); y = (float) kOut.tick (kc, y); }   // K-weighted
                in += w * (x * x - in); out += w * (y * y - out);
            }
        }
        inline float target (int mode) const
        {
            const float r = mode == 1 ? in / juce::jmax (1.0e-6f, out) : std::sqrt (in / juce::jmax (1.0e-9f, out));
            return juce::jlimit (0.125f, 8.f, r);
        }
        inline float step (int mode, double rate) { g += (1.f - std::exp (-1.f / (0.04f * (float) rate))) * (target (mode) - g); return g; }
    };
    struct Meter { float inPk = 0, outPk = 0, inMs = 0, outMs = 0, gr = 0, grMax = 0, grSum = 0; int grN = 0; bool clip = false; };
    inline void meterSample (Meter& m, float in, float out, float grDb)
    {
        m.inPk = juce::jmax (m.inPk, std::abs (in)); m.outPk = juce::jmax (m.outPk, std::abs (out));
        m.inMs += 0.0005f * (in * in - m.inMs); m.outMs += 0.0005f * (out * out - m.outMs);
        m.gr = grDb; m.grMax = juce::jmin (m.grMax, grDb); m.grSum += grDb; ++m.grN;
        m.clip |= std::abs (out) > 1.f;
    }

    // ------------------------------------------------------------------ COMPRESSOR
    struct CmpSettings
    {
        bool on = false; int loc = 0, pos = 1, type = VCA, det = DPeak, arel = ArManual, sc = 0;
        float thr = -18.f, ratio = 4.f, atk = 10.f, rel = 120.f, knee = 6.f, inG = 1.f, outG = 1.f, mix = 1.f, dry = 1.f, wet = 1.f, link = 1.f, scGain = 1.f, scMix = 1.f;
        int look = 0; tone::Coef scHp, scLp; bool hp = false, lp = false;
        float aCo = 0.1f, rCo = 0.01f, rmsCo = 0.01f;
    };
    // the static curve (dB in -> dB out): the same function the audio uses (also drawn by the GUI)
    inline float cmpGainDb (const CmpSettings& s, float lvl)
    {
        const float over = lvl - s.thr;
        switch (s.type)
        {
            case Upward:  { if (lvl < -80.f) return 0.f; return juce::jmin (24.f, tone::kneeOver (-over, s.knee) * (1.f - 1.f / s.ratio)); }
            case VariMu:  { const float r = 1.f + (s.ratio - 1.f) * juce::jlimit (0.f, 1.f, (over + 0.5f * s.knee) / 36.f); return -tone::kneeOver (over, juce::jmax (12.f, s.knee)) * (1.f - 1.f / r); }   // the tube: the ratio grows with the level
            case Opto:    return -tone::kneeOver (over, juce::jmax (6.f, s.knee)) * (1.f - 1.f / s.ratio);
            default:      return -tone::kneeOver (over, s.knee) * (1.f - 1.f / s.ratio);
        }
    }
    struct CmpState
    {
        std::array<float, 2> env {}, ms {}, grDb {}, lin { 1.f, 1.f }, lastOut {};
        std::array<TruePeak, 2> tp {};
        tone::Sec hp, lp;
        float hist = 0.f, held = 0.f;
        std::array<std::array<float, maxLook>, 2> dl {}; int di = 0;
        Level lv; Meter meter;
        void reset() { *this = CmpState(); }
        inline float detect (const CmpSettings& s, int ch, float x)
        {
            const float a = std::abs (x);
            switch (s.det)
            {
                case DRms:    ms[(size_t) ch] += s.rmsCo * (x * x - ms[(size_t) ch]); return std::sqrt (ms[(size_t) ch]) * 1.4142f;
                case DHybrid: ms[(size_t) ch] += s.rmsCo * (x * x - ms[(size_t) ch]); return juce::jmax (0.7f * a, std::sqrt (ms[(size_t) ch]) * 1.4142f);
                case DTrue:   return tp[(size_t) ch].push (x, 4);
                default:      return a;
            }
        }
        // sc = the sidechain source sample (mono), -1e9 when SELF
        inline void process (const CmpSettings& s, float& l, float& r, float sc, int agMode, bool match, double rate)
        {
            const float il = l * s.inG, ir = r * s.inG;
            // ---- detector input: own signal (FET: the previous output), sidechain source (filtered, gain, mix)
            float dl0 = s.type == FET ? lastOut[0] : il, dr0 = s.type == FET ? lastOut[1] : ir;
            if (sc > -1.0e8f)
            {
                double v = sc * s.scGain;
                if (s.hp) v = hp.tick (s.scHp, v);
                if (s.lp) v = lp.tick (s.scLp, v);
                const float m = s.type == Sidechain ? 1.f : s.scMix;
                dl0 = dl0 + ((float) v - dl0) * m; dr0 = dr0 + ((float) v - dr0) * m;
            }
            else if (s.hp || s.lp)
            {
                double v = 0.5 * (dl0 + dr0);
                if (s.hp) v = hp.tick (s.scHp, v);
                if (s.lp) v = lp.tick (s.scLp, v);
                dl0 = dr0 = (float) v;
            }
            float lev[2] { detect (s, 0, dl0), detect (s, 1, dr0) };
            const float mx = juce::jmax (lev[0], lev[1]);
            for (auto& v : lev) v = v + (mx - v) * s.link;
            if (s.type == Sidechain) { held = juce::jmax (lev[0], held * 0.9995f); lev[0] = lev[1] = held; }   // a short hold: clean ducking
            // ---- release: manual / auto / program dependent / OPTO (history)
            float rCo = s.rCo;
            switch (s.arel)
            {
                case ArFast:   rCo = coefMs (50.f, rate); break;
                case ArMedium: rCo = coefMs (200.f, rate); break;
                case ArSlow:   rCo = coefMs (800.f, rate); break;
                case ArProgram:rCo = coefMs (40.f + 760.f * juce::jlimit (0.f, 1.f, -hist / 10.f), rate); break;
                default: break;
            }
            if (s.type == Opto) rCo = coefMs ((1.f + 3.f * juce::jlimit (0.f, 1.f, -hist / 8.f)) / juce::jmax (1.0e-6f, rCo) / (float) rate * 1000.f, rate);
            const float aCo = s.type == Opto ? juce::jmin (s.aCo, coefMs (10.f, rate)) : s.type == FET ? juce::jmax (s.aCo, coefMs (0.05f, rate))
                            : s.type == VariMu ? juce::jmin (s.aCo, coefMs (3.f, rate)) : s.aCo;   // (VARI-MU never grabs faster than 3 ms)
            float g[2];
            const bool same = lev[0] == lev[1] && grDb[0] == grDb[1] && lin[0] == lin[1];   // (linked: one channel's math is enough)
            for (int c = 0; c < (same ? 1 : 2); ++c)
            {
                const float target = cmpGainDb (s, fastDb (lev[c]));
                if (s.type == Digital)                                              // linear-domain envelope (a different curve shape)
                {
                    const float tl = fromDb (target);
                    lin[(size_t) c] += (tl < lin[(size_t) c] ? aCo : rCo) * (tl - lin[(size_t) c]);
                    g[c] = lin[(size_t) c]; grDb[(size_t) c] = fastDb (g[c]);
                }
                else
                {
                    auto& e = grDb[(size_t) c];
                    const bool more = s.type == Upward ? target > e : target < e;
                    e += (more ? aCo : rCo) * (target - e);
                    g[c] = fromDb (e);
                }
            }
            if (same) { g[1] = g[0]; grDb[1] = grDb[0]; lin[1] = lin[0]; }
            hist += 0.0001f * (juce::jmin (grDb[0], grDb[1]) - hist);
            // ---- the audio path (LOOKAHEAD = the audio waits, the detector does not)
            float al = il, ar = ir;
            if (s.look > 0)
            {
                dl[0][(size_t) di] = il; dl[1][(size_t) di] = ir;
                const int rd = (di - s.look + maxLook) % maxLook;
                al = dl[0][(size_t) rd]; ar = dl[1][(size_t) rd];
                di = (di + 1) % maxLook;
            }
            float wl = al * g[0], wr = ar * g[1];
            if (s.type == FET)                                                    // the FET gain cell colours what it compresses
            {
                const float amt = juce::jlimit (0.f, 1.f, -juce::jmin (grDb[0], grDb[1]) / 12.f) * 0.25f;
                wl += amt * (std::tanh (2.f * wl) * 0.5f - wl); wr += amt * (std::tanh (2.f * wr) * 0.5f - wr);
            }
            float yl, yr;
            if (s.type == Parallel)
            {
                const float n = 1.f / juce::jmax (1.f, s.dry + s.wet);              // dry + wet never doubles the level
                yl = (al * s.dry + wl * s.wet) * n; yr = (ar * s.dry + wr * s.wet) * n;
            }
            else { yl = wl; yr = wr; }
            yl = al + (yl - al) * s.mix; yr = ar + (yr - ar) * s.mix;
            yl *= s.outG; yr *= s.outG;
            lastOut[0] = yl; lastOut[1] = yr;
            finish (s.look, al, ar, yl, yr, agMode, match, rate);
            meterSample (meter, 0.5f * (il + ir), 0.5f * (yl + yr), juce::jmin (grDb[0], grDb[1]));
            l = std::isfinite (yl) ? yl : 0.f; r = std::isfinite (yr) ? yr : 0.f;
        }
        inline void finish (int, float al, float ar, float& yl, float& yr, int agMode, bool match, double rate)
        {
            if (agMode == 0 && ! match) return;
            lv.measure (agMode == 0 ? 2 : agMode, 0.5f * (al + ar), 0.5f * (yl + yr), rate);
            if (match) { const float k = 1.f / lv.step (agMode == 0 ? 2 : agMode, rate); yl = al * k; yr = ar * k; return; }   // LEVEL MATCH: the dry signal at the processed level
            const float k = lv.step (agMode, rate); yl *= k; yr *= k;
        }
    };

    // ------------------------------------------------------------------ TRANSIENT SHAPER
    struct TsSettings { bool on = false; int loc = 1, pos = 1, det = TdFast, mode = TmPunch; float atk = 0.f, sus = 0.f, sens = 0.5f, time = 20.f, rel = 100.f, mix = 1.f, outG = 1.f;
                        float fA = 0.1f, fR = 0.01f, sA = 0.01f, sR = 0.001f, gR = 0.01f; };
    struct TsState
    {
        float fast = 0.f, slow = 0.f, g = 0.f; Level lv; Meter meter;
        void reset() { *this = TsState(); }
        inline void process (const TsSettings& s, float& l, float& r, const Ctx& cx, int agMode, bool match, double rate)
        {
            const float m = 0.5f * (l + r);
            const float x = s.det == TdRms ? m * m : std::abs (m);
            fast += (x > fast ? s.fA : s.fR) * (x - fast);
            slow += (x > slow ? s.sA : s.sR) * (x - slow);
            const float ef = s.det == TdRms ? std::sqrt (fast) : fast, es = s.det == TdRms ? std::sqrt (slow) : slow;
            const float d = fastDb ((ef + 1.0e-9f) / (es + 1.0e-9f));               // > 0 = an attack is happening
            const float range = 2.f + (1.f - s.sens) * 10.f;                        // SENSITIVITY: how big a jump counts as transient
            float att = juce::jlimit (0.f, 1.f, d / range), sus = juce::jlimit (0.f, 1.f, (-d + 0.5f) / range + (d < 0.5f ? 0.3f : 0.f));
            // MODES use the kick's own time (since the hit): PUNCH = the first TIME only, BODY = TIME .. 4 TIME, TAIL = after 2 TIME
            switch (s.mode)
            {
                case TmPunch:   if (cx.ms > s.time * 2.f) att = 0.f; break;
                case TmSustain: att *= 0.3f; break;
                case TmBody:    { const bool in = cx.ms > s.time * 0.5f && cx.ms < s.time * 4.f; att *= in ? 1.f : 0.2f; sus *= in ? 1.f : 0.3f; break; }
                default:        if (cx.ms < s.time * 2.f) { att = 0.f; sus = 0.f; } break;
            }
            const float targetDb = s.atk * 18.f * att + s.sus * 18.f * sus;
            g += (std::abs (targetDb) > std::abs (g) ? 0.25f : s.gR) * (targetDb - g);
            const float k = fromDb (g);
            float yl = l * k, yr = r * k;
            yl = (l + (yl - l) * s.mix) * s.outG; yr = (r + (yr - r) * s.mix) * s.outG;
            if (agMode != 0 || match)
            {
                lv.measure (agMode == 0 ? 2 : agMode, m, 0.5f * (yl + yr), rate);
                if (match) { const float q = 1.f / lv.step (agMode == 0 ? 2 : agMode, rate); yl = l * q; yr = r * q; }
                else { const float q = lv.step (agMode, rate); yl *= q; yr *= q; }
            }
            meterSample (meter, m, 0.5f * (yl + yr), g);
            l = yl; r = yr;
        }
    };

    // ------------------------------------------------------------------ CLIPPER
    struct ClpSettings { bool on = false; int loc = 0, pos = 1, type = CSoft, os = 0; float inG = 1.f, ceil = 0.89f, drive = 0.3f, sym = 0.f, soft = 0.3f, mix = 1.f, outG = 1.f; const dist::CurveData* curve = nullptr; };
    // the static curve: input (linear) -> output (linear), ceiling included - the audio and the GUI curve use this
    inline float clipCurve (const ClpSettings& s, float x)
    {
        const float g = s.inG * (1.f + 15.f * s.drive * s.drive);
        float u = x * g / s.ceil;
        const float pos = 1.f + 0.6f * juce::jmax (0.f, s.sym), neg = 1.f - 0.6f * juce::jmin (0.f, s.sym);   // SYMMETRY: the halves are hit differently
        u *= u >= 0.f ? pos : neg;
        const float k = juce::jlimit (0.f, 0.95f, s.soft);
        float y;
        switch (s.type)
        {
            case CHard: { const float a = std::abs (u), kn = 0.5f * k; y = a <= 1.f - kn ? a : a >= 1.f + kn ? 1.f : a - (a - (1.f - kn)) * (a - (1.f - kn)) / (4.f * juce::jmax (1.0e-4f, kn)); y = std::copysign (y, u); break; }
            case CAsym: { const float c = u >= 0.f ? 1.f : 0.6f + 0.4f * (1.f - k); y = juce::jlimit (-c, 1.f, u); y = y + (std::tanh (u) - y) * k * 0.5f; y = juce::jlimit (-1.f, 1.f, y); break; }
            case CSine: { const float a = juce::jmin (1.f, std::abs (u) * (1.f - 0.3f * k)); y = std::copysign (std::sin (a * juce::MathConstants<float>::halfPi), u); break; }
            case CCubic:{ const float a = juce::jmin (1.f, std::abs (u) / (1.f + k)); y = std::copysign (1.5f * a - 0.5f * a * a * a, u); break; }
            case CTanh: { const float d = 1.f + 3.f * (1.f - k); y = std::tanh (u * d) / std::tanh (d); y = juce::jlimit (-1.f, 1.f, y); break; }
            case CCustom: y = s.curve != nullptr ? juce::jlimit (-1.f, 1.f, s.curve->eval (juce::jlimit (-1.f, 1.f, u), 1)) : juce::jlimit (-1.f, 1.f, u); break;
            default:    { const float a = std::abs (u), kn = juce::jmax (0.02f, k);                             // SOFT: linear, then a smooth knee into the ceiling
                          y = a <= 1.f - kn ? a : 1.f - kn + kn * std::tanh ((a - (1.f - kn)) / kn); y = std::copysign (juce::jmin (1.f, y), u); break; }
        }
        return y * s.ceil;
    }
    struct ClpState
    {
        dist::Halfband<31> h1L, h1R; dist::Halfband<33> h2L, h2R;
        std::array<float, 64> dlyL {}, dlyR {}; int di = 0;
        Level lv; Meter meter;
        void reset() { *this = ClpState(); }
        inline float run (const ClpSettings& s, float x, dist::Halfband<31>& h1, dist::Halfband<33>& h2)
        {
            if (s.os == 0) return clipCurve (s, x);
            float a, b; h1.upsample (x, a, b);
            float ya, yb;
            if (s.os == 1) { ya = clipCurve (s, a); yb = clipCurve (s, b); }
            else
            {
                float a1, a2, b1, b2; h2.upsample (a, a1, a2); h2.upsample (b, b1, b2);
                const float c1 = clipCurve (s, a1), c2 = clipCurve (s, a2), c3 = clipCurve (s, b1), c4 = clipCurve (s, b2);
                ya = h2.downsample (c1, c2); yb = h2.downsample (c3, c4);
            }
            return h1.downsample (ya, yb);
        }
        inline void process (const ClpSettings& s, float& l, float& r, int agMode, bool match, double rate)
        {
            float yl = run (s, l, h1L, h2L), yr = run (s, r, h1R, h2R);
            const int lat = s.os == 0 ? 0 : s.os == 1 ? dist::Halfband<31>::latency2x() / 2 : dist::Halfband<31>::latency2x() / 2 + dist::Halfband<33>::latency2x() / 4;
            dlyL[(size_t) di] = l; dlyR[(size_t) di] = r;
            const float dl = dlyL[(size_t) ((di - lat + 64) % 64)], dr = dlyR[(size_t) ((di - lat + 64) % 64)];
            di = (di + 1) % 64;
            yl = (dl + (yl - dl) * s.mix) * s.outG; yr = (dr + (yr - dr) * s.mix) * s.outG;
            if (agMode != 0 || match)
            {
                lv.measure (agMode == 0 ? 2 : agMode, 0.5f * (dl + dr), 0.5f * (yl + yr), rate);
                if (match) { const float q = 1.f / lv.step (agMode == 0 ? 2 : agMode, rate); yl = dl * q; yr = dr * q; }
                else { const float q = lv.step (agMode, rate); yl *= q; yr *= q; }
            }
            meterSample (meter, 0.5f * (l + r), 0.5f * (yl + yr), fastDb ((std::abs (0.5f * (yl + yr)) + 1.0e-9f) / (std::abs (0.5f * (dl + dr)) * s.outG + 1.0e-9f)));
            l = std::isfinite (yl) ? yl : 0.f; r = std::isfinite (yr) ? yr : 0.f;
        }
    };

    // ------------------------------------------------------------------ LIMITER
    struct LimSettings { bool on = false; int loc = LMaster, pos = 1, mode = LTransparent, os = 2; float ceil = 0.966f, inG = 1.f, rel = 60.f, atk = 0.f, knee = 1.f, link = 1.f, mix = 1.f; bool tp = false; int look = 0;
                         float rCo = 0.001f, aCo = 1.f; };
    // a running minimum over the last LOOK samples (monotonic queue, fixed arrays)
    struct MinQ
    {
        std::array<float, maxLook> v {}; std::array<int64_t, maxLook> at {}; int h = 0, n = 0;
        inline float push (float x, int64_t t, int look)
        {
            while (n > 0 && v[(size_t) ((h + n - 1) % maxLook)] >= x) --n;
            const int k = (h + n) % maxLook; v[(size_t) k] = x; at[(size_t) k] = t; ++n;
            while (n > 0 && t - at[(size_t) h] > look) { h = (h + 1) % maxLook; --n; }
            return v[(size_t) h];
        }
    };
    struct LimState
    {
        std::array<std::array<float, maxLook>, 2> dl {}; int di = 0; int64_t t = 0;
        std::array<MinQ, 2> q {};
        std::array<TruePeak, 2> tp {};
        std::array<float, 2> g { 1.f, 1.f }; Level lv; Meter meter;
        void reset() { *this = LimState(); }
        static inline float need (const LimSettings& s, float pk)
        {
            if (pk <= 1.0e-9f) return 1.f;
            const float over = fastDb (pk / s.ceil);
            const float gr = s.knee > 0.f ? (over >= 0.5f * s.knee ? over : over > -0.5f * s.knee ? (over + 0.5f * s.knee) * (over + 0.5f * s.knee) / (2.f * s.knee) : 0.f) : juce::jmax (0.f, over);
            return juce::jmin (fromDb (-gr), pk > s.ceil ? s.ceil / pk : 1.f);
        }
        inline void process (const LimSettings& s, float& l, float& r, int agMode, bool match, double rate)
        {
            float in[2] { l * s.inG, r * s.inG };
            if (s.mode == LLoud) for (auto& x : in) x = std::tanh (x * 1.3f) / 1.3f * 1.15f;   // LOUD: a soft push into the limiter
            const int steps = (s.tp || s.mode == LTrue) ? (s.os <= 0 ? 1 : 1 << s.os) : 1;
            float pk[2] { steps > 1 ? tp[0].push (in[0], steps) : std::abs (in[0]), steps > 1 ? tp[1].push (in[1], steps) : std::abs (in[1]) };
            // STEREO LINK: 100 % = both channels get the gain the louder needs, 0 % = each channel its own
            float req[2] { need (s, pk[0]), need (s, pk[1]) };
            const float lnk = juce::jmin (req[0], req[1]);
            for (auto& v : req) v += (lnk - v) * s.link;
            float a[2] { in[0], in[1] };
            if (s.look > 0)
            {
                for (int c = 0; c < 2; ++c) { req[c] = q[(size_t) c].push (req[c], t, s.look); dl[(size_t) c][(size_t) di] = in[c]; }
                const int rd = (di - s.look + maxLook) % maxLook;
                a[0] = dl[0][(size_t) rd]; a[1] = dl[1][(size_t) rd];
                di = (di + 1) % maxLook; ++t;
            }
            const float aCo = s.look > 0 ? juce::jmax (s.aCo, 1.f - std::exp (-4.f / (float) juce::jmax (1, s.look))) : s.aCo;
            float y[2];
            for (int c = 0; c < 2; ++c)
            {
                float rCo = s.rCo;
                if (s.mode == LTransparent) rCo *= 0.4f + 0.6f * juce::jlimit (0.f, 1.f, 1.f + fastDb (g[(size_t) c]) / 6.f);   // deeper reduction = slower release (no pumping)
                if (s.mode == LAggressive) rCo = juce::jmin (1.f, rCo * 3.f);
                auto& gc = g[(size_t) c];
                const bool down = req[c] < gc;
                gc += (down ? aCo : rCo) * (req[c] - gc);
                // PUNCH: while the gain still has to come down the transient snaps through (the guard below keeps the ceiling)
                y[c] = a[c] * (s.mode == LPunch && down ? juce::jmin (1.f, gc * 1.4f) : gc);
                y[c] = juce::jlimit (-s.ceil, s.ceil, y[c]);                        // the guard: the ceiling is never passed
                y[c] = a[c] + (y[c] - a[c]) * s.mix;
            }
            if (agMode != 0 || match)
            {
                lv.measure (agMode == 0 ? 2 : agMode, 0.5f * (a[0] + a[1]), 0.5f * (y[0] + y[1]), rate);
                if (match) { const float k = 1.f / lv.step (agMode == 0 ? 2 : agMode, rate); y[0] = a[0] * k; y[1] = a[1] * k; }
                else { const float k = juce::jmin (1.f, lv.step (agMode, rate)); y[0] *= k; y[1] *= k; }   // (auto gain never lifts past the ceiling)
            }
            meterSample (meter, 0.5f * (in[0] + in[1]), 0.5f * (y[0] + y[1]), fastDb (juce::jmin (g[0], g[1])));
            l = std::isfinite (y[0]) ? y[0] : 0.f; r = std::isfinite (y[1]) ? y[1] : 0.f;
        }
    };

    // ------------------------------------------------------------------ CREST FACTOR CONTROL
    struct CrestSettings { bool on = false; int loc = 0, pos = 1; float target = 12.f, punch = 0.f, body = 0.f, tail = 0.f, sens = 0.5f, mix = 1.f; float pkR = 0.001f, rmsCo = 0.001f; };
    struct CrestState
    {
        float pk = 0.f, ms = 0.f, g = 0.f; Meter meter;
        void reset() { *this = CrestState(); }
        inline void process (const CrestSettings& s, float& l, float& r, const Ctx& cx)
        {
            const float m = 0.5f * (l + r), a = std::abs (m);
            pk = juce::jmax (a, pk * (1.f - s.pkR));
            ms += s.rmsCo * (m * m - ms);
            const float crest = fastDb ((pk + 1.0e-9f) / (std::sqrt (ms) * 1.4142f + 1.0e-9f));
            // how much of a peak this sample is (0 = the average, 1 = the peak itself)
            const float peakness = juce::jlimit (0.f, 1.f, fastDb ((a + 1.0e-9f) / (std::sqrt (ms) * 1.4142f + 1.0e-9f)) / juce::jmax (1.f, crest));
            const float zone = cx.ms < 30.f ? s.punch : cx.ms < 150.f ? s.body : s.tail;
            const float err = crest - s.target;                                     // > 0 = too peaky: pull the peaks down
            const float targetDb = (-err * s.sens * 0.8f + zone * 9.f) * peakness;
            g += 0.02f * (juce::jlimit (-18.f, 12.f, targetDb) - g);
            const float k = fromDb (g);
            const float yl = l + (l * k - l) * s.mix, yr = r + (r * k - r) * s.mix;
            meterSample (meter, m, 0.5f * (yl + yr), g);
            l = yl; r = yr;
        }
    };

    // ------------------------------------------------------------------ all of it cooked + the order per place
    struct Bank
    {
        std::array<CmpSettings, numCmp> cmp {}; std::array<TsSettings, numTs> ts {}; std::array<ClpSettings, numClp> clp {}; std::array<LimSettings, numLim> lim {};
        CrestSettings crest; int autoGain = 0, order = 0; bool match = false, any = false;
        std::array<std::array<uint64_t, 2>, NumLocs> mask {};                        // which processors sit at [loc][pos] (bit = kind * 8 + index)
        int masterLook = 0;                                                          // samples of lookahead at MASTER OUTPUT (the latency it adds)
        bool at (int loc, int pos) const { return mask[(size_t) loc][(size_t) pos] != 0; }
        bool atLoc (int loc) const { return at (loc, 0) || at (loc, 1); }
    };
    struct Probe { int kind = -1, index = 0; std::vector<float> gr, in, out; };     // (offline analysis only: the GUI's history)
    struct State
    {
        std::array<CmpState, numCmp> cmp; std::array<TsState, numTs> ts; std::array<ClpState, numClp> clp; std::array<LimState, numLim> lim; CrestState crest;
        Probe* probe = nullptr;
        void reset() { for (auto& x : cmp) x.reset(); for (auto& x : ts) x.reset(); for (auto& x : clp) x.reset(); for (auto& x : lim) x.reset(); crest.reset(); }
        // sc[0..5]: SELF (unused), PUNCH, BODY, TAIL, SUB, TEXTURE (the raw sources this sample)
        inline void process (const Bank& b, int loc, int pos, float& l, float& r, const Ctx& cx, const float* sc, double rate)
        {
            const uint64_t m = b.mask[(size_t) loc][(size_t) pos];
            auto bit = [&] (int kind, int i) { return (m >> (kind * 8 + i)) & 1ull; };
            auto rec = [&] (int kind, int i, float in, float gr)
            {
                if (probe != nullptr && probe->kind == kind && probe->index == i) { probe->gr.push_back (gr); probe->in.push_back (in); probe->out.push_back (0.5f * (l + r)); }
            };
            auto runCmp = [&] { for (int i = 0; i < numCmp; ++i) if (bit (KCmp, i)) { const auto& s = b.cmp[(size_t) i]; const float in = 0.5f * (l + r);
                                  cmp[(size_t) i].process (s, l, r, s.sc > 0 && sc != nullptr ? sc[s.sc] : -1.0e9f, b.autoGain, b.match, rate);
                                  rec (KCmp, i, in, juce::jmin (cmp[(size_t) i].grDb[0], cmp[(size_t) i].grDb[1])); } };
            auto runTs = [&] { for (int i = 0; i < numTs; ++i) if (bit (KTs, i)) { const float in = 0.5f * (l + r); ts[(size_t) i].process (b.ts[(size_t) i], l, r, cx, b.autoGain, b.match, rate); rec (KTs, i, in, ts[(size_t) i].g); } };
            auto runClp = [&] { for (int i = 0; i < numClp; ++i) if (bit (KClp, i)) { const float in = 0.5f * (l + r); clp[(size_t) i].process (b.clp[(size_t) i], l, r, b.autoGain, b.match, rate); rec (KClp, i, in, clp[(size_t) i].meter.gr); } };
            auto runLim = [&] { for (int i = 0; i < numLim; ++i) if (bit (KLim, i)) { const float in = 0.5f * (l + r); lim[(size_t) i].process (b.lim[(size_t) i], l, r, b.autoGain, b.match, rate); rec (KLim, i, in, fastDb (juce::jmin (lim[(size_t) i].g[0], lim[(size_t) i].g[1]))); } };
            auto runCr = [&] { if (bit (KCrest, 0)) { const float in = 0.5f * (l + r); crest.process (b.crest, l, r, cx); rec (KCrest, 0, in, crest.g); } };
            switch (b.order)
            {
                case 1:  runTs(); runCmp(); runClp(); break;
                case 2:  runCmp(); runClp(); runTs(); break;
                case 3:  runClp(); runCmp(); runTs(); break;
                default: runCmp(); runTs(); runClp(); break;
            }
            runLim(); runCr();
        }
    };
}
