#pragma once
// =====================================================================
//  4GE module 07 - EQ, MULTIBAND & SPECTRAL SHAPING (pure DSP: no GUI, no files, no allocation after construction).
//  One processor class for every place (GLOBAL / PUNCH / BODY / TAIL / SUB / TEXTURE / LAYER 1-8 / SCREECH), BEFORE or AFTER
//  that place's distortion (module 05):
//   PARAMETRIC EQ  - 16 bands in a pool: BELL, LOW / HIGH SHELF, LOW / HIGH CUT (6-48 dB), BAND PASS, NOTCH, TILT, RESONANT PEAK
//                    (TPT state variable sections - stable at any Q, smooth under automation), stereo / L / R / mid / side, mix;
//   DYNAMIC EQ     - 4 bands: a bell whose gain follows a real detector (peak / RMS, attack / release, knee, ratio, range),
//                    modes CUT / BOOST / UPWARD / DOWNWARD, detector on this band / the full signal / the lows / the highs;
//   MULTIBAND      - 4 bands (LR 12 / 24 / 48, Butterworth, Bessel; all-pass compensated), per band gain, width, invert, mute,
//                    solo, dynamics (compressor / expander / gate / limiter / transient control on a real envelope detector);
//   SPECTRAL       - tilt, harmonic shaper (even / odd, low / high band), exciter (6 curves, new harmonics only in its range),
//                    spectral compression (24-band dynamic filter bank: only what sticks out is pulled down, no latency),
//                    spectral freeze (the band energies captured at a time, resynthesised with decay / pitch shift);
//   STEREO         - mid / side in 3 bands: width per band, mid / side gains, MONO BELOW always before the width, mono listen.
// =====================================================================
#include "../Filter/Filters.h"
#include <array>
#include <cmath>
#include <complex>

namespace fge::dsp::tone
{
    using m06::Ctx;
    using m06::NumLocs;
    enum EqType { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, BandPass, Notch, Tilt, ResPeak, NumEqTypes };
    enum EqStereo { EsStereo = 0, EsLeft, EsRight, EsMid, EsSide };
    enum DeqMode { DCut = 0, DBoost, DUpward, DDownward };
    enum DeqSrc { DsBand = 0, DsFull, DsLows, DsHighs };
    enum XType { XLr12 = 0, XLr24, XLr48, XButter, XBessel };
    enum MbDyn { MdOff = 0, MdComp, MdExpand, MdGate, MdLimit, MdTransient };
    enum ExcMode { XmWarm = 0, XmBright, XmCrisp, XmMetallic, XmAggressive, XmCustom };
    constexpr int numEq = 16, numDeq = 4, numMb = 4, numSpecBands = 24;
    constexpr int stwLocs[7] { m06::LGlobal, m06::LPunch, m06::LBody, m06::LTail, m06::LSub, m06::LTexture, m06::LScreech };
    inline bool eqHasGain (int t) { return t == Bell || t == LowShelf || t == HighShelf || t == Tilt || t == ResPeak; }
    inline bool eqHasQ (int t) { return t != Tilt; }
    inline bool eqHasSlope (int t) { return t == LowCut || t == HighCut || t == BandPass || t == Notch; }

    // ------------------------------------------------------------------ one TPT SVF section (Simper): all RBJ shapes, double state
    struct Coef { double a1 = 1, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0; };
    struct Sec
    {
        double ic1 = 0, ic2 = 0;
        inline double tick (const Coef& c, double v0)
        {
            const double v3 = v0 - ic2, v1 = c.a1 * ic1 + c.a2 * v3, v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
            return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
        }
    };
    enum Shape { SLp = 0, SHp, SBp, SNotch, SAp, SBell, SLowShelf, SHighShelf, SBpUnity };
    inline Coef coef (int shape, double hz, double q, double db, double rate)
    {
        hz = juce::jlimit (5.0, rate * 0.49, hz); q = juce::jlimit (0.05, 60.0, q);
        double g = std::tan (juce::MathConstants<double>::pi * hz / rate), k = 1.0 / q;
        const double A = std::pow (10.0, db / 40.0);
        Coef c;
        if (shape == SBell) k = 1.0 / (q * A);
        if (shape == SLowShelf) g /= std::sqrt (A);
        if (shape == SHighShelf) g *= std::sqrt (A);
        c.a1 = 1.0 / (1.0 + g * (g + k)); c.a2 = g * c.a1; c.a3 = g * c.a2;
        switch (shape)
        {
            case SLp:        c.m0 = 0; c.m1 = 0; c.m2 = 1; break;
            case SHp:        c.m0 = 1; c.m1 = -k; c.m2 = -1; break;
            case SBp:        c.m0 = 0; c.m1 = 1; c.m2 = 0; break;
            case SBpUnity:   c.m0 = 0; c.m1 = k; c.m2 = 0; break;                       // 0 dB at the centre
            case SNotch:     c.m0 = 1; c.m1 = -k; c.m2 = 0; break;
            case SAp:        c.m0 = 1; c.m1 = -2 * k; c.m2 = 0; break;
            case SBell:      c.m0 = 1; c.m1 = k * (A * A - 1); c.m2 = 0; break;
            case SLowShelf:  c.m0 = 1; c.m1 = k * (A - 1); c.m2 = A * A - 1; break;
            case SHighShelf: c.m0 = A * A; c.m1 = k * (1 - A) * A; c.m2 = 1 - A * A; break;
            default: break;
        }
        return c;
    }
    // one pole TPT (6 dB cuts, LR12 crossovers)
    struct One { double s = 0; inline double lp (double g, double x) { const double v = (x - s) * g / (1 + g); const double y = v + s; s = y + v; return y; } };
    inline double onePoleG (double hz, double rate) { return std::tan (juce::MathConstants<double>::pi * juce::jlimit (5.0, rate * 0.49, hz) / rate); }

    // ------------------------------------------------------------------ PARAMETRIC EQ band
    struct EqSettings { bool on = false; int loc = 0, pos = 1, type = Bell, slope = 1, stereo = EsStereo; float freq = 1000.f, gain = 0.f, q = 0.9f, mix = 1.f; };
    struct EqState
    {
        std::array<std::array<Sec, 4>, 2> s {};   // [channel][section]
        std::array<std::array<Sec, 2>, 2> t {};   // tilt shelves
        std::array<One, 2> one {};
        std::array<Coef, 4> c {}; std::array<Coef, 2> tc {}; double g1 = 0.1;
        int n = 1, tick = 0; float f = -1.f, gn = 0.f, qq = 1.f, mix = 1.f; float kf = 0.f, kg = 0.f, kq = 0.f; int kt = -1, ks = -1;
        void reset() { *this = EqState(); }
        void update (const EqSettings& e, double rate)
        {
            // smoothing (~10 ms): frequency in octaves, gain in dB
            if (f < 0.f) { f = e.freq; gn = e.gain; qq = e.q; mix = e.mix; }
            const float a = 0.12f;
            f = std::exp2 (std::log2 (f) + a * (std::log2 (e.freq) - std::log2 (f))); gn += a * (e.gain - gn); qq += a * (e.q - qq); mix += a * (e.mix - mix);
            if (std::abs (f - kf) < 1.0e-4f * kf && std::abs (gn - kg) < 1.0e-4f && std::abs (qq - kq) < 1.0e-5f && kt == e.type && ks == e.slope) return;
            kf = f; kg = gn; kq = qq; kt = e.type; ks = e.slope;
            static const double bw4[2] { 0.5412, 1.3066 }, bw8[4] { 0.5098, 0.6013, 0.9000, 2.5629 };
            const double res = qq / 0.7071;                                                  // the cut's Q = resonance on top of Butterworth
            switch (e.type)
            {
                case Bell:      n = 1; c[0] = coef (SBell, f, qq, gn, rate); break;
                case LowShelf:  n = 1; c[0] = coef (SLowShelf, f, juce::jmin (qq, 2.f), gn, rate); break;
                case HighShelf: n = 1; c[0] = coef (SHighShelf, f, juce::jmin (qq, 2.f), gn, rate); break;
                case LowCut: case HighCut:
                {
                    const int sh = e.type == LowCut ? SHp : SLp;
                    if (e.slope == 0) { n = 0; g1 = onePoleG (f, rate); break; }
                    if (e.slope == 1) { n = 1; c[0] = coef (sh, f, qq, 0, rate); break; }
                    if (e.slope == 2) { n = 2; c[0] = coef (sh, f, bw4[0], 0, rate); c[1] = coef (sh, f, bw4[1] * res, 0, rate); break; }
                    n = 4; for (int k = 0; k < 4; ++k) c[(size_t) k] = coef (sh, f, k == 3 ? bw8[3] * res : bw8[k], 0, rate); break;
                }
                case BandPass: { n = e.slope <= 1 ? 1 : e.slope == 2 ? 2 : 4; for (int k = 0; k < n; ++k) c[(size_t) k] = coef (SBpUnity, f, qq, 0, rate); break; }
                case Notch:    { n = e.slope <= 1 ? 1 : e.slope == 2 ? 2 : 4; for (int k = 0; k < n; ++k) c[(size_t) k] = coef (SNotch, f, qq, 0, rate); break; }
                case Tilt:     { n = 0; tc[0] = coef (SLowShelf, f, 0.5, -0.5 * gn, rate); tc[1] = coef (SHighShelf, f, 0.5, 0.5 * gn, rate); break; }
                case ResPeak:  { n = 1; c[0] = coef (SBpUnity, f, qq * 3.0, 0, rate); break; }
                default: break;
            }
        }
        inline double run (const EqSettings& e, int ch, double x)
        {
            auto& sc = s[(size_t) ch];
            switch (e.type)
            {
                case Tilt: { auto& tt = t[(size_t) ch]; return tt[1].tick (tc[1], tt[0].tick (tc[0], x)); }
                case ResPeak: return x + (std::pow (10.0, gn / 20.0) - 1.0) * sc[0].tick (c[0], x);
                case LowCut: case HighCut:
                    if (n == 0) { const double l = one[(size_t) ch].lp (g1, x); return e.type == LowCut ? x - l : l; }
                    [[fallthrough]];
                default: { double y = x; for (int k = 0; k < n; ++k) y = sc[(size_t) k].tick (c[(size_t) k], y); return y; }
            }
        }
        inline void process (const EqSettings& e, float& l, float& r, double rate)
        {
            if ((tick++ & 31) == 0) update (e, rate);
            const float dl = l, dr = r;
            switch (e.stereo)
            {
                case EsLeft:  l = (float) run (e, 0, l); break;
                case EsRight: r = (float) run (e, 1, r); break;
                case EsMid:   { const double m = run (e, 0, 0.5 * (l + r)), sd = 0.5 * (l - r); l = (float) (m + sd); r = (float) (m - sd); break; }
                case EsSide:  { const double m = 0.5 * (l + r), sd = run (e, 1, 0.5 * (l - r)); l = (float) (m + sd); r = (float) (m - sd); break; }
                default:      l = (float) run (e, 0, l); r = (float) run (e, 1, r); break;
            }
            l = dl + (l - dl) * mix; r = dr + (r - dr) * mix;
        }
    };
    // the static response of a cooked band (dB at hz) - for the GUI curve (computed off the audio thread)
    inline double coefMagDb (const Coef& c, double hz, double rate)
    {
        // H(z) of the TPT SVF evaluated through its analog prototype (bilinear, prewarped): exact at the cooked rate
        const double w = std::tan (juce::MathConstants<double>::pi * juce::jlimit (1.0, rate * 0.499, hz) / rate);
        const double g = c.a2 / c.a1;                                          // a2 = g a1
        const double k = (1.0 / c.a1 - 1.0 - g * g) / g;                       // a1 = 1 / (1 + g (g + k))
        // v1 / v0 = (s / g) / (s^2 / g^2 + k s / g + 1) ; v2 / v0 = 1 / (s^2 / g^2 + k s / g + 1) with s = j w
        const std::complex<double> s (0.0, w / g);
        const std::complex<double> den = s * s + k * s + 1.0;
        const std::complex<double> h = c.m0 + c.m1 * (s / den) + c.m2 * (1.0 / den);
        return 20.0 * std::log10 (std::abs (h) + 1.0e-12);
    }

    // ------------------------------------------------------------------ DYNAMIC EQ band
    struct DeqSettings { bool on = false; int loc = 0, pos = 1, mode = DCut, src = DsBand, det = 0; float win = 5.f, freq = 3500.f, q = 2.f, thr = -24.f, range = -9.f, ratio = 4.f, knee = 6.f, atk = 2.f, rel = 80.f, mix = 1.f; };
    inline float kneeOver (float over, float knee)                 // soft knee: how far over the threshold counts
    {
        if (knee <= 0.01f) return juce::jmax (0.f, over);
        if (over <= -0.5f * knee) return 0.f;
        if (over >= 0.5f * knee) return over;
        const float u = over + 0.5f * knee; return u * u / (2.f * knee);
    }
    struct DeqState
    {
        Sec det, det2; std::array<Sec, 2> eq {}; Coef dc, dc2, ec; float env = 0.f, ms = 0.f, gainDb = 0.f, gNow = 0.f; int tick = 0;
        float aCo = 0.f, rCo = 0.f, wCo = 0.f; int src = -1; float kf = -1.f, kq = -1.f;
        void reset() { *this = DeqState(); }
        inline void process (const DeqSettings& d, float& l, float& r, double rate)
        {
            if ((tick & 15) == 0)
            {
                if (src != d.src || kf != d.freq || kq != d.q)
                {
                    src = d.src; kf = d.freq; kq = d.q;
                    if (d.src == DsBand) dc = coef (SBpUnity, d.freq, d.q, 0, rate);
                    else if (d.src == DsLows) { dc = coef (SLp, 200.0, 0.7071, 0, rate); dc2 = dc; }
                    else if (d.src == DsHighs) { dc = coef (SHp, 3000.0, 0.7071, 0, rate); dc2 = dc; }
                }
                aCo = 1.f - std::exp (-1.f / (0.001f * d.atk * (float) rate)); rCo = 1.f - std::exp (-1.f / (0.001f * d.rel * (float) rate));
                wCo = 1.f - std::exp (-1.f / (0.001f * d.win * (float) rate));
                // the gain computer (dB)
                const float lvl = juce::Decibels::gainToDecibels (env, -120.f);
                const float slope = 1.f - 1.f / juce::jmax (1.f, d.ratio), mx = std::abs (d.range);
                float g = 0.f;
                switch (d.mode)
                {
                    case DCut:      g = -juce::jmin (mx, kneeOver (lvl - d.thr, d.knee) * slope); break;
                    case DBoost:    g =  juce::jmin (mx, kneeOver (lvl - d.thr, d.knee) * slope); break;
                    case DUpward:   g =  juce::jmin (mx, kneeOver (d.thr - lvl, d.knee) * slope); break;
                    default:        g = -juce::jmin (mx, kneeOver (d.thr - lvl, d.knee) * slope); break;
                }
                gainDb = g;
                gNow += 0.35f * (gainDb - gNow);
                ec = coef (SBell, d.freq, d.q, gNow, rate);
            }
            ++tick;
            // ---- detector (stereo linked)
            const double m = 0.5 * (l + r);
            double sc = m;
            if (d.src == DsBand) sc = det.tick (dc, m);
            else if (d.src != DsFull) sc = det2.tick (dc2, det.tick (dc, m));
            float lev = (float) std::abs (sc);
            if (d.det == 1) { ms += wCo * (lev * lev - ms); lev = std::sqrt (ms) * 1.4142f; }   // RMS (a sine reads as its peak)
            env += (lev > env ? aCo : rCo) * (lev - env);
            const float dl = l, dr = r;
            l = (float) eq[0].tick (ec, l); r = (float) eq[1].tick (ec, r);
            l = dl + (l - dl) * d.mix; r = dr + (r - dr) * d.mix;
        }
    };

    // ------------------------------------------------------------------ crossovers (one split = low + high, low + high = all-pass for LR)
    struct XSet { int type = XLr24; Coef lp[4], hp[4]; double g1 = 0.1; int n = 2; };
    inline void cookX (XSet& x, int type, double hz, double rate)
    {
        x.type = type;
        static const double bw4[2] { 0.5412, 1.3066 };
        switch (type)
        {
            case XLr12: x.n = 0; x.g1 = onePoleG (hz, rate); break;
            case XLr24: x.n = 2; for (int k = 0; k < 2; ++k) { x.lp[k] = coef (SLp, hz, 0.7071, 0, rate); x.hp[k] = coef (SHp, hz, 0.7071, 0, rate); } break;
            case XLr48: x.n = 4; for (int k = 0; k < 4; ++k) { x.lp[k] = coef (SLp, hz, bw4[k & 1], 0, rate); x.hp[k] = coef (SHp, hz, bw4[k & 1], 0, rate); } break;
            case XButter: x.n = 1; x.lp[0] = coef (SLp, hz, 0.7071, 0, rate); x.hp[0] = coef (SHp, hz, 0.7071, 0, rate); break;
            default: x.n = 1; x.lp[0] = coef (SLp, hz, 0.5773, 0, rate); x.hp[0] = coef (SHp, hz, 0.5773, 0, rate); break;   // Bessel
        }
    }
    struct XState
    {
        Sec l[4], h[4]; One o1, o2, o3, o4;
        inline void split (const XSet& x, double in, double& lo, double& hi)
        {
            if (x.n == 0)                                                   // LR12: one pole squared, the high band inverted
            {
                const double a = o1.lp (x.g1, in), b = o2.lp (x.g1, a);
                const double ha = in - o3.lp (x.g1, in), hb = ha - o4.lp (x.g1, ha);
                lo = b; hi = -hb; return;
            }
            lo = in; hi = in;
            for (int k = 0; k < x.n; ++k) { lo = l[k].tick (x.lp[k], lo); hi = h[k].tick (x.hp[k], hi); }
        }
        inline double allpass (const XSet& x, double in) { double a, b; split (x, in, a, b); return a + b; }
    };

    // ------------------------------------------------------------------ MULTIBAND
    struct MbBand { float gain = 1.f, width = 1.f; bool inv = false, mute = false, solo = false; int dyn = MdOff; float thr = -18.f, ratio = 4.f, atk = 5.f, rel = 120.f, knee = 6.f, makeup = 1.f, mix = 1.f, trA = 0.f, trS = 0.f; };
    struct MbSettings { bool on = false; int loc = 0, pos = 1, listen = 0; std::array<XSet, 3> x; std::array<MbBand, numMb> b; bool anySolo = false; };
    struct MbState
    {
        std::array<std::array<XState, 6>, 2> xs {};                 // [ch]: split 1 / 2 / 3, all-pass b0@2, b0@3, b1@3
        std::array<float, numMb> env {}, fast {}, slow {}, gNow {};
        std::array<float, numMb> gain {}, width {}; bool first = true;
        void reset() { *this = MbState(); }
        inline void process (const MbSettings& s, float& l, float& r, double rate)
        {
            double bl[numMb], br[numMb];
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& X = xs[(size_t) ch]; double* o = ch == 0 ? bl : br;
                const double in = ch == 0 ? l : r;
                double b0, rest, b1, rest2, b2, b3;
                X[0].split (s.x[0], in, b0, rest);
                X[1].split (s.x[1], rest, b1, rest2);
                X[2].split (s.x[2], rest2, b2, b3);
                b0 = X[3].allpass (s.x[1], b0); b0 = X[4].allpass (s.x[2], b0); b1 = X[5].allpass (s.x[2], b1);
                o[0] = b0; o[1] = b1; o[2] = b2; o[3] = b3;
            }
            if (first) { for (int k = 0; k < numMb; ++k) { gain[(size_t) k] = s.b[(size_t) k].gain; width[(size_t) k] = s.b[(size_t) k].width; } first = false; }
            double yl = 0, yr = 0;
            for (int k = 0; k < numMb; ++k)
            {
                const auto& b = s.b[(size_t) k];
                if (b.mute || (s.anySolo && ! b.solo)) continue;
                double L = bl[k], R = br[k];
                if (s.listen == 0)
                {
                    gain[(size_t) k] += 0.002f * (b.gain - gain[(size_t) k]); width[(size_t) k] += 0.002f * (b.width - width[(size_t) k]);
                    // ---- dynamics on a real envelope (stereo linked)
                    if (b.dyn != MdOff)
                    {
                        const float lev = (float) juce::jmax (std::abs (L), std::abs (R));
                        float g = 0.f;
                        if (b.dyn == MdTransient)
                        {
                            const float fa = 1.f - std::exp (-1.f / (0.0005f * (float) rate)), fr = 1.f - std::exp (-1.f / (0.02f * (float) rate));
                            const float sa = 1.f - std::exp (-1.f / (0.001f * juce::jmax (5.f, b.atk) * 4.f * (float) rate)), sr = 1.f - std::exp (-1.f / (0.001f * b.rel * (float) rate));
                            fast[(size_t) k] += (lev > fast[(size_t) k] ? fa : fr) * (lev - fast[(size_t) k]);
                            slow[(size_t) k] += (lev > slow[(size_t) k] ? sa : sr) * (lev - slow[(size_t) k]);
                            const float d = juce::Decibels::gainToDecibels (fast[(size_t) k], -100.f) - juce::Decibels::gainToDecibels (slow[(size_t) k], -100.f);
                            g = b.trA * juce::jlimit (0.f, 1.f, d / 6.f) + b.trS * juce::jlimit (0.f, 1.f, -d / 6.f + (d < 1.f ? 0.5f : 0.f));
                        }
                        else
                        {
                            const float a = 1.f - std::exp (-1.f / (0.001f * b.atk * (float) rate)), rl = 1.f - std::exp (-1.f / (0.001f * b.rel * (float) rate));
                            env[(size_t) k] += (lev > env[(size_t) k] ? a : rl) * (lev - env[(size_t) k]);
                            const float lv = juce::Decibels::gainToDecibels (env[(size_t) k], -120.f);
                            switch (b.dyn)
                            {
                                case MdComp:   g = -kneeOver (lv - b.thr, b.knee) * (1.f - 1.f / b.ratio); break;
                                case MdLimit:  g = -kneeOver (lv - b.thr, juce::jmin (b.knee, 2.f)); break;
                                case MdExpand: g = -juce::jmin (60.f, kneeOver (b.thr - lv, b.knee) * (b.ratio - 1.f)); break;
                                default:       g = -juce::jmin (60.f, kneeOver (b.thr - lv, b.knee) * 20.f); break;     // GATE
                            }
                        }
                        gNow[(size_t) k] += 0.05f * (g - gNow[(size_t) k]);
                        const double gg = std::pow (10.0, gNow[(size_t) k] / 20.0) * b.makeup;
                        L = L + (L * gg - L) * b.mix; R = R + (R * gg - R) * b.mix;
                    }
                    const double m = 0.5 * (L + R), sd = 0.5 * (L - R) * width[(size_t) k];
                    L = (m + sd) * gain[(size_t) k] * (b.inv ? -1 : 1); R = (m - sd) * gain[(size_t) k] * (b.inv ? -1 : 1);
                }
                yl += L; yr += R;
            }
            l = (float) yl; r = (float) yr;
        }
    };

    // ------------------------------------------------------------------ SPECTRAL SHAPER
    struct SpecSettings
    {
        bool on = false; int loc = 0, pos = 1;
        float tilt = 0.f, pivot = 1000.f, tiltInt = 0.5f, tiltMix = 1.f;
        bool hOn = false; float hEven = 0.5f, hOdd = 0.5f, hLow = 0.5f, hHigh = 0.5f, hSplit = 800.f, hDrive = 0.4f, hMix = 0.5f, hBal = 0.f;
        bool xOn = false; int xMode = XmBright, xHarm = 2; float xLo = 2500.f, xHi = 12000.f, xAmt = 0.4f, xColor = 0.5f, xDrive = 0.5f, xMix = 1.f;
        bool cOn = false; float cSens = 0.5f, cThr = 6.f, cRed = 9.f, cAtk = 3.f, cRel = 80.f, cLo = 300.f, cHi = 12000.f, cSmooth = 0.5f, cMix = 1.f;
        bool fOn = false; float fCap = 150.f, fDecay = 600.f, fSmooth = 0.3f, fPitch = 0.f, fMix = 0.5f;
        // cooked
        Coef tLo, tHi; XSet hX; Coef xHp, xLp, xHp2; std::array<Coef, numSpecBands> band, synth; float aCo = 0.01f, rCo = 0.001f, tone = 0.1f;
    };
    struct SpecState
    {
        std::array<std::array<Sec, 2>, 2> tilt {};
        std::array<XState, 2> hx {}; std::array<float, 4> hdc {}, hdcY {}; std::array<double, 2> hTone {};
        std::array<std::array<Sec, 3>, 2> xs {}; std::array<double, 2> xTone {};
        std::array<std::array<Sec, numSpecBands>, 2> cb {};
        std::array<float, numSpecBands> cEnv {}, cGr {}, frozen {}; bool captured = false; float lastMs = -1.f;
        std::array<Sec, numSpecBands> syn {}; uint32_t rng = 0x2545F491u; float fadeIn = 0.f;
        int tick = 0;
        void reset() { *this = SpecState(); }
        inline float noise() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (int32_t) rng * (1.f / 2147483648.f); }
        // even (squared, DC removed) + odd (soft) harmonics of one signal
        static inline double harm (double u, float drive, float even, float odd)
        {
            const double g = 1.0 + 12.0 * drive;
            const double o = std::tanh (g * u) / g - u;                     // odd: what the soft curve adds / takes
            const double e = 0.5 * g * u * u;                              // even: the squared part (DC removed later)
            return odd * o + even * e;
        }
        static inline double excCurve (int mode, double u)
        {
            switch (mode)
            {
                case XmWarm:       return std::tanh (u + 0.35 * u * u);
                case XmCrisp:      return juce::jlimit (-1.0, 1.0, u);
                case XmMetallic:   return std::sin (2.4 * u);
                case XmAggressive: { double v = u; for (int i = 0; i < 4 && std::abs (v) > 0.6; ++i) v = std::copysign (1.2 - std::abs (v), v); return v / 0.6; }
                case XmCustom:     return u - 0.3 * u * u * u + 0.2 * u * u;
                default:           return std::tanh (u);                     // BRIGHT (with pre-emphasis)
            }
        }
        inline void process (const SpecSettings& s, float& l, float& r, const Ctx& cx, double rate)
        {
            double io[2] { l, r };
            // ---- spectral compression / freeze analysis (24 dynamic band passes)
            const bool bank = s.cOn || s.fOn;
            if (bank)
            {
                const double m = 0.5 * (io[0] + io[1]);
                if ((tick & 15) == 0 && s.cOn)
                {
                    float mean = 0.f; for (auto e : cEnv) mean += juce::Decibels::gainToDecibels (e, -100.f); mean /= (float) numSpecBands;
                    for (int k = 0; k < numSpecBands; ++k)
                    {
                        const float ex = juce::Decibels::gainToDecibels (cEnv[(size_t) k], -100.f) - mean - s.cThr;
                        const float g = juce::jlimit (0.f, s.cRed, ex * (0.25f + 1.75f * s.cSens));
                        cGr[(size_t) k] += 0.3f * (g - cGr[(size_t) k]);
                    }
                }
                ++tick;
                double sub[2] { 0, 0 };
                for (int k = 0; k < numSpecBands; ++k)
                {
                    const double bL = cb[0][(size_t) k].tick (s.band[(size_t) k], io[0]), bR = cb[1][(size_t) k].tick (s.band[(size_t) k], io[1]);
                    const float lev = (float) std::abs (0.5 * (bL + bR));
                    auto& e = cEnv[(size_t) k]; e += (lev > e ? s.aCo : s.rCo) * (lev - e);
                    if (s.cOn) { const double cut = 1.0 - std::pow (10.0, -cGr[(size_t) k] / 20.0); sub[0] += cut * bL; sub[1] += cut * bR; }
                }
                if (s.cOn) for (int c = 0; c < 2; ++c) io[c] -= sub[c] * s.cMix;
                // ---- FREEZE: capture the band energies once, then resynthesise them (noise through the bands, decay, pitch)
                if (s.fOn)
                {
                    if (cx.ms < lastMs) { captured = false; fadeIn = 0.f; }
                    lastMs = cx.ms;
                    if (! captured && cx.ms >= s.fCap)
                    {
                        captured = true;
                        for (int k = 0; k < numSpecBands; ++k)                    // SMOOTHING: across neighbouring bands
                        {
                            float a = 0.f, w = 0.f;
                            for (int j = -2; j <= 2; ++j) { const int q = juce::jlimit (0, numSpecBands - 1, k + j); const float wt = j == 0 ? 1.f : s.fSmooth * (j == 1 || j == -1 ? 0.8f : 0.4f); a += wt * cEnv[(size_t) q]; w += wt; }
                            frozen[(size_t) k] = a / w;
                        }
                    }
                    if (captured)
                    {
                        const float dec = std::exp (-(cx.ms - s.fCap) / juce::jmax (10.f, s.fDecay));
                        fadeIn = juce::jmin (1.f, fadeIn + 0.0005f);
                        double y = 0;
                        for (int k = 0; k < numSpecBands; ++k) y += syn[(size_t) k].tick (s.synth[(size_t) k], noise()) * frozen[(size_t) k] * 3.0;
                        y *= dec;
                        const float mx = s.fMix * fadeIn;
                        for (int c = 0; c < 2; ++c) io[c] = io[c] * (1.0 - mx) + y * mx;
                    }
                }
                juce::ignoreUnused (m);
            }
            for (int c = 0; c < 2; ++c)
            {
                double x = io[c];
                // ---- TILT (two opposite shelves around the pivot)
                if (s.tilt != 0.f) { const double d = x; x = tilt[(size_t) c][1].tick (s.tHi, tilt[(size_t) c][0].tick (s.tLo, x)); x = d + (x - d) * s.tiltMix; }
                // ---- HARMONIC SHAPER: new even / odd harmonics in the low and the high band (+ BALANCE tone on them)
                if (s.hOn)
                {
                    double lo, hi; hx[(size_t) c].split (s.hX, x, lo, hi);
                    double h = s.hLow * harm (lo, s.hDrive, s.hEven, s.hOdd) + s.hHigh * harm (hi, s.hDrive, s.hEven, s.hOdd);
                    // DC (from the even part) out: 10 Hz blocker
                    const float rDc = 1.f - (float) (juce::MathConstants<double>::twoPi * 10.0 / rate);
                    const float hv = (float) h; const float d = hv - hdc[(size_t) c] + rDc * hdcY[(size_t) c]; hdc[(size_t) c] = hv; hdcY[(size_t) c] = d; h = d;
                    hTone[(size_t) c] += s.tone * (h - hTone[(size_t) c]);
                    h = s.hBal >= 0.f ? h + s.hBal * ((h - hTone[(size_t) c]) - h) : h + (-s.hBal) * (hTone[(size_t) c] - h);
                    x += h * s.hMix;
                }
                // ---- EXCITER: only new content inside its range
                if (s.xOn)
                {
                    auto& X = xs[(size_t) c];
                    double b = X[1].tick (s.xLp, X[0].tick (s.xHp, x));
                    if (s.xMode == XmBright) b *= 1.6;
                    const double g = 1.0 + 15.0 * s.xDrive;
                    // the curve's EVEN part / ODD part minus the clean signal / both: only what the curve ADDS is used
                    const double p = excCurve (s.xMode, g * b) / g, n = excCurve (s.xMode, -g * b) / g;
                    double y = s.xHarm == 0 ? 0.5 * (p + n) : s.xHarm == 1 ? 0.5 * (p - n) - b : p - b;
                    y = X[2].tick (s.xHp2, y * 3.0);                               // keep it above the range's bottom (no low rumble)
                    xTone[(size_t) c] += 0.2 * (y - xTone[(size_t) c]);
                    y = xTone[(size_t) c] + (y - xTone[(size_t) c]) * (0.3 + 1.4 * s.xColor);   // COLOR: dark .. bright
                    x += y * s.xAmt * s.xMix;
                }
                io[c] = std::isfinite (x) ? x : 0.0;
            }
            l = (float) io[0]; r = (float) io[1];
        }
    };

    // ------------------------------------------------------------------ STEREO FREQUENCY ENGINE
    struct StwSettings
    {
        bool on = false; int loc = 0; float width = 1.f, lowW = 1.f, midW = 1.f, highW = 1.f, mix = 1.f; bool monoListen = false;
        std::array<float, 3> midG { 1, 1, 1 }, sideG { 1, 1, 1 }; bool midNeutral = true;
        XSet xLow, xHigh, mono;
    };
    struct StwState
    {
        XState mLo, mHi, mAp, sLo, sHi, sAp, mono; float w = 1.f; bool first = true;
        void reset() { *this = StwState(); }
        inline void process (const StwSettings& s, float& l, float& r)
        {
            const double dl = l, dr = r;
            double m = 0.5 * (l + r), sd = 0.5 * (l - r);
            // MONO BELOW always first: the side signal loses everything below (WIDTH can never widen the sub)
            double a, b; mono.split (s.mono, sd, a, b); sd = b;
            if (first) { w = s.width; first = false; }
            w += 0.002f * (s.width - w);
            double s0, sr0, s1, s2; sLo.split (s.xLow, sd, s0, sr0); sHi.split (s.xHigh, sr0, s1, s2); s0 = sAp.allpass (s.xHigh, s0);
            sd = w * (s0 * s.lowW * s.sideG[0] + s1 * s.midW * s.sideG[1] + s2 * s.highW * s.sideG[2]);
            if (! s.midNeutral)
            {
                double m0, mr0, m1, m2; mLo.split (s.xLow, m, m0, mr0); mHi.split (s.xHigh, mr0, m1, m2); m0 = mAp.allpass (s.xHigh, m0);
                m = m0 * s.midG[0] + m1 * s.midG[1] + m2 * s.midG[2];
            }
            double L = m + sd, R = m - sd;
            if (s.monoListen) L = R = 0.5 * (L + R);
            l = (float) (dl + (L - dl) * s.mix); r = (float) (dr + (R - dr) * s.mix);
        }
    };

    // ------------------------------------------------------------------ everything cooked (per block) + the order per place
    struct Bank
    {
        std::array<EqSettings, numEq> eq {};
        std::array<DeqSettings, numDeq> deq {};
        MbSettings mb; SpecSettings sp; StwSettings stw;
        std::array<std::array<std::array<int, numEq>, 2>, NumLocs> eqList {};
        std::array<std::array<int, 2>, NumLocs> eqCount {};
        std::array<std::array<std::array<int, numDeq>, 2>, NumLocs> deqList {};
        std::array<std::array<int, 2>, NumLocs> deqCount {};
        int order = 0; bool any = false;
        bool at (int loc, int pos) const
        {
            return eqCount[(size_t) loc][(size_t) pos] + deqCount[(size_t) loc][(size_t) pos] > 0 || (mb.on && mb.loc == loc && mb.pos == pos)
                || (sp.on && sp.loc == loc && sp.pos == pos) || (pos == 1 && stw.on && stw.loc == loc);
        }
        bool atLoc (int loc) const { return at (loc, 0) || at (loc, 1); }
    };
    struct State
    {
        std::array<EqState, numEq> eq;
        std::array<DeqState, numDeq> deq;
        MbState mb; SpecState sp; StwState stw;
        void reset() { for (auto& e : eq) e.reset(); for (auto& d : deq) d.reset(); mb.reset(); sp.reset(); stw.reset(); }
        inline void process (const Bank& b, int loc, int pos, float& l, float& r, const Ctx& cx, double rate)
        {
            auto runEq = [&] { const int n = b.eqCount[(size_t) loc][(size_t) pos]; for (int k = 0; k < n; ++k) { const int i = b.eqList[(size_t) loc][(size_t) pos][(size_t) k]; eq[(size_t) i].process (b.eq[(size_t) i], l, r, rate); } };
            auto runDeq = [&] { const int n = b.deqCount[(size_t) loc][(size_t) pos]; for (int k = 0; k < n; ++k) { const int i = b.deqList[(size_t) loc][(size_t) pos][(size_t) k]; deq[(size_t) i].process (b.deq[(size_t) i], l, r, rate); } };
            auto runMb = [&] { if (b.mb.on && b.mb.loc == loc && b.mb.pos == pos) mb.process (b.mb, l, r, rate); };
            auto runSp = [&] { if (b.sp.on && b.sp.loc == loc && b.sp.pos == pos) sp.process (b.sp, l, r, cx, rate); };
            switch (b.order)
            {
                case 1:  runDeq(); runEq(); runMb(); runSp(); break;
                case 2:  runMb(); runEq(); runDeq(); runSp(); break;
                case 3:  runSp(); runEq(); runDeq(); runMb(); break;
                default: runEq(); runDeq(); runMb(); runSp(); break;
            }
            if (pos == 1 && b.stw.on && b.stw.loc == loc) stw.process (b.stw, l, r);
        }
    };
}
