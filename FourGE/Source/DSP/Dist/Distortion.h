#pragma once
// =====================================================================
//  4GE MODULE 05 - DISTORTION & WAVESHAPER ENGINE
//  A pool of 16 STAGES (sized for more). Each stage sits at a LOCATION - GLOBAL (the whole kick), a GROUP (PUNCH / BODY / TAIL /
//  SUB / TEXTURE bus) or one LAYER (before its pan / fader) - in an ORDER. Consecutive stages build blocks: SERIAL,
//  PARALLEL (branches A / B / C, summed with level compensation) or MULTIBAND (LR4 3-band split, phase-compensated, each band its
//  own stages, band solo).
//  A stage: PRE FILTER -> input gain x DRIVE (curve) + BIAS -> transfer function -> POST FILTER (mix) -> output / AUTO GAIN / MIX.
//  The 11 transfer functions are real, different curves (soft / hard / tube / diode / wavefold / bitcrush / rectifier / asymmetric /
//  foldback / digital / custom). Static curves are tabulated per setting together with their integral -> 1st order ADAA
//  (antiderivative anti-aliasing) on top of the engine's 4x; a stage can add 2x / 4x more (FIR halfband, dry path delay-matched).
//  MORPH crossfades two algorithms run in parallel (no jump). The modulation envelope (own / drawn / pitch / input level /
//  velocity) moves drive, bias, threshold, fold / shape, mix, output and the post cutoff in time.
//  The transfer table the GUI draws is the same table the DSP uses.
// =====================================================================
#include "../Env/MultiEnv.h"
#include "DoomsdayDSP.h"
#include <array>
#include <vector>

namespace fge::dsp::dist
{
    enum Type { Soft = 0, Hard, Tube, Diode, Wavefold, Bitcrush, Rectifier, Asym, Foldback, Digital, Custom, NumTypes };
    enum Loc { LocGlobal = 0, LocPunch, LocBody, LocTail, LocSub, LocTexture, LocLayer1, NumLocs = 14 };
    enum Path { PathSerial = 0, PathA, PathB, PathC };
    enum Band { BandFull = 0, BandLow, BandMid, BandHigh };
    enum FType { FLow = 0, FHigh, FBand, FNotch, FLadder };
    enum ModSrc { MsOwn = 0, MsDrawn, MsPitch, MsInput, MsVelocity };
    constexpr int numStages = 16, tableN = 8192, maxCurvePoints = 128;

    // ------------------------------------------------------------------ CUSTOM curve (2..128 points, x -1..1 -> y -1..1)
    struct CurveData
    {
        int n = 2;
        std::array<float, maxCurvePoints> x {}, y {};
        CurveData() { x[0] = -1.f; y[0] = -1.f; x[1] = 1.f; y[1] = 1.f; }
        static CurveData preset (int kind)                         // 0 identity, 1 s-curve, 2 asymmetric, 3 steps
        {
            CurveData c; c.n = 0;
            auto add = [&] (float a, float b) { c.x[(size_t) c.n] = a; c.y[(size_t) c.n] = b; ++c.n; };
            if (kind == 1) { add (-1, -1); add (-0.4f, -0.85f); add (0, 0); add (0.4f, 0.85f); add (1, 1); }
            else if (kind == 2) { add (-1, -0.5f); add (-0.3f, -0.4f); add (0, 0); add (0.25f, 0.8f); add (1, 1); }
            else if (kind == 3) { add (-1, -1); add (-0.5f, -0.5f); add (-0.45f, 0.1f); add (0, 0); add (0.45f, -0.1f); add (0.5f, 0.5f); add (1, 1); }
            else { add (-1, -1); add (1, 1); }
            return c;
        }
        void sanitize()
        {
            n = juce::jlimit (2, maxCurvePoints, n);
            x[0] = -1.f; x[(size_t) n - 1] = 1.f;
            for (int i = 0; i < n; ++i)
            {
                auto& a = x[(size_t) i]; auto& b = y[(size_t) i];
                a = std::isfinite (a) ? juce::jlimit (i == 0 ? -1.f : x[(size_t) i - 1], 1.f, a) : 0.f;
                b = std::isfinite (b) ? juce::jlimit (-1.5f, 1.5f, b) : 0.f;          // never more than +3.5 dB of gain
            }
        }
        // interpolation: 0 LINEAR, 1 CUBIC (monotone Hermite, no overshoot), 2 BEZIER (smooth: quadratic through midpoints)
        float eval (float t, int interp) const
        {
            t = juce::jlimit (-1.f, 1.f, t);
            int i = 0;
            while (i < n - 2 && x[(size_t) i + 1] < t) ++i;
            const float x0 = x[(size_t) i], x1 = x[(size_t) i + 1], y0 = y[(size_t) i], y1 = y[(size_t) i + 1];
            const float w = juce::jmax (1.0e-6f, x1 - x0), u = juce::jlimit (0.f, 1.f, (t - x0) / w);
            if (interp == 0 || n == 2) return y0 + (y1 - y0) * u;
            auto slope = [&] (int k)
            {
                if (k <= 0) return (y[1] - y[0]) / juce::jmax (1.0e-6f, x[1] - x[0]);
                if (k >= n - 1) return (y[(size_t) n - 1] - y[(size_t) n - 2]) / juce::jmax (1.0e-6f, x[(size_t) n - 1] - x[(size_t) n - 2]);
                const float a = (y[(size_t) k] - y[(size_t) k - 1]) / juce::jmax (1.0e-6f, x[(size_t) k] - x[(size_t) k - 1]);
                const float b = (y[(size_t) k + 1] - y[(size_t) k]) / juce::jmax (1.0e-6f, x[(size_t) k + 1] - x[(size_t) k]);
                return a * b <= 0.f ? 0.f : 2.f * a * b / (a + b);           // monotone (Fritsch-Carlson style)
            };
            if (interp == 1)
            {
                const float m0 = slope (i) * w, m1 = slope (i + 1) * w, u2 = u * u, u3 = u2 * u;
                return (2 * u3 - 3 * u2 + 1) * y0 + (u3 - 2 * u2 + u) * m0 + (-2 * u3 + 3 * u2) * y1 + (u3 - u2) * m1;
            }
            // BEZIER: the segment's control point is the crossing of the neighbour tangents (clamped inside the segment box)
            const float c = juce::jlimit (juce::jmin (y0, y1), juce::jmax (y0, y1), 0.5f * (y0 + slope (i) * w * 0.5f) + 0.5f * (y1 - slope (i + 1) * w * 0.5f));
            return (1 - u) * (1 - u) * y0 + 2 * (1 - u) * u * c + u * u * y1;
        }
        juce::String toString() const
        {
            juce::String s (n);
            for (int i = 0; i < n; ++i) s << ";" << juce::String (x[(size_t) i], 4) << "," << juce::String (y[(size_t) i], 4);
            return s;
        }
        static bool fromString (const juce::String& s, CurveData& out)
        {
            auto p = juce::StringArray::fromTokens (s, ";", "");
            if (p.size() < 3) return false;
            CurveData c; c.n = juce::jlimit (2, maxCurvePoints, p[0].getIntValue());
            if (p.size() < c.n + 1) return false;
            for (int i = 0; i < c.n; ++i) { auto q = juce::StringArray::fromTokens (p[i + 1], ",", ""); c.x[(size_t) i] = q[0].getFloatValue(); c.y[(size_t) i] = q[1].getFloatValue(); }
            c.sanitize(); out = c; return true;
        }
    };
    struct DistData
    {
        std::array<CurveData, numStages> curve; std::array<int, numStages> curveVersion {};
        DistData() { for (auto& c : curve) c = CurveData::preset (1); }       // a gentle S-curve to start from (same as the processor)
    };

    // ------------------------------------------------------------------ the static curve of one setting
    struct ShapeParams
    {
        int type = Soft, rect = 1, interp = 1;
        float thr = 0.8f, shape = 0.5f, ceil = 0.7f, even = 0.4f, odd = 0.8f, stages = 3.f, negDrive = 0.7f, negThr = 0.5f, sym = 0.f;
        const CurveData* curve = nullptr; int curveVer = 0;
        bool operator== (const ShapeParams& o) const
        {
            return type == o.type && rect == o.rect && interp == o.interp && thr == o.thr && shape == o.shape && ceil == o.ceil && even == o.even
                && odd == o.odd && stages == o.stages && negDrive == o.negDrive && negThr == o.negThr && sym == o.sym && curve == o.curve && (type != Custom || curveVer == o.curveVer);
        }
    };
    // y = f(x): x is the driven input (any size), y about -1..1 for the clipping types. Every branch is its own transfer function.
    inline float transfer (const ShapeParams& p, float x)
    {
        // SYMMETRY (all but HARD / ASYMMETRIC, which have their own): the two halves are driven differently
        if (p.type != Hard && p.type != Asym) x *= x > 0.f ? 1.f + 0.7f * p.sym : 1.f - 0.7f * p.sym;
        const float T = 0.05f + 0.95f * p.thr;
        switch (p.type)
        {
            case Soft:                                                   // linear up to the knee, then a smooth bend into THRESHOLD
            {
                const float a = T * (1.f - juce::jlimit (0.02f, 1.f, p.shape)), ax = std::abs (x);
                const float y = ax <= a ? ax : a + (T - a) * std::tanh ((ax - a) / juce::jmax (1.0e-4f, T - a));
                return std::copysign (y / T, x);
            }
            case Hard:                                                   // a straight wall at CEILING (SYMMETRY moves the halves)
            {
                const float pc = p.ceil * (p.sym < 0.f ? 1.f + 0.8f * p.sym : 1.f), nc = p.ceil * (p.sym > 0.f ? 1.f - 0.8f * p.sym : 1.f);
                return juce::jlimit (-nc, pc, x) / p.ceil;
            }
            case Tube:                                                   // odd (tanh) + even (an even curve) parts + WARMTH rounding
            {
                const float o = std::tanh (x), e = 1.f - std::exp (-x * x * 0.8f);
                float y = p.odd * o + p.even * 0.7f * e;
                const float warm = std::atan (1.2f * x) / std::atan (1.2f);
                y = y + (juce::jlimit (-1.2f, 1.2f, warm) * (p.odd + 0.3f * p.even) - y) * p.shape * 0.6f;
                return y;
            }
            case Diode:                                                  // a soft exponential top, a tight bottom (ASYMMETRY)
            {
                const float tn = T * (1.f - 0.75f * p.shape);
                return x >= 0.f ? (1.f - std::exp (-x / T)) : -(tn / T) * std::tanh (-x / tn);
            }
            case Wavefold:                                               // sine folding, the number of folds limited by STAGES
            {
                const float lim = 2.f * std::round (p.stages) - 1.f;
                float u = x / T * (1.f + 3.f * p.shape);
                u = juce::jlimit (-lim, lim, u);
                return std::sin (juce::MathConstants<float>::halfPi * u);
            }
            case Rectifier:
            {
                float r;
                switch (p.rect)
                {
                    case 0:  r = x > 0.f ? x : 0.f; break;                  // HALF WAVE
                    case 2:  r = x > 0.f ? x : 0.2f * x; break;            // POSITIVE (keeps a little of the bottom)
                    case 3:  r = x < 0.f ? x : 0.2f * x; break;            // NEGATIVE
                    case 4:  r = x > 0.f ? x : -x * (p.shape * 2.f - 1.f); break;   // CUSTOM: the bottom flipped by SHAPE
                    default: r = std::abs (x); break;                      // FULL WAVE (an octave up)
                }
                const float y = x + (r - x) * juce::jlimit (0.f, 1.f, p.shape * 1.4f);
                return std::tanh (y);
            }
            case Asym:                                                   // positive / negative halves: own drive, own threshold
            {
                const float tp = 0.05f + 0.95f * p.thr, tn = 0.05f + 0.95f * p.negThr, gn = 0.25f + 3.f * p.negDrive;
                return x >= 0.f ? tp * std::tanh (x / tp) / tp : tn * std::tanh (x * gn / tn) / tn;
            }
            case Foldback:                                               // reflection at the threshold (FOLD AMOUNT = how steep it comes back)
            {
                float u = x;
                const float k = 0.3f + 1.7f * p.shape;
                for (int i = 0; i < 6 && std::abs (u) > T; ++i) u = std::copysign (T - (std::abs (u) - T) * k, u);
                return juce::jlimit (-T, T, u) / T;
            }
            case Digital:                                                // CEILING + HARDNESS: a cubic knee into a hard wall
            {
                const float u = x / p.ceil, h = juce::jlimit (0.f, 1.f, p.shape);
                const float soft = std::abs (u) < 1.5f ? u - (4.f / 27.f) * u * u * u : std::copysign (1.f, u);
                const float hard = juce::jlimit (-1.f, 1.f, u);
                return soft + (hard - soft) * h;
            }
            case Custom: return p.curve != nullptr ? p.curve->eval (juce::jlimit (-1.f, 1.f, x), p.interp) : juce::jlimit (-1.f, 1.f, x);
            default: return x;                                           // (BITCRUSH is not a static curve)
        }
    }

    // ------------------------------------------------------------------ tabulated curve + its integral (for ADAA)
    struct Table
    {
        float R = 4.f; std::vector<float> f; std::vector<double> F;
        ShapeParams key; float keyR = -1.f; uint32_t serial = 0;        // serial: +1 on every rebuild (ADAA / de-click follow it)
        void allocate() { f.assign (tableN + 1, 0.f); F.assign (tableN + 1, 0.0); }
        // rebuilt only when the setting changes (no allocation: the vectors are sized once)
        void build (const ShapeParams& p, float range)
        {
            if (f.size() != (size_t) tableN + 1) allocate();
            if (p == key && range == keyR) return;
            key = p; keyR = range; R = range; ++serial;
            const double dx = 2.0 * R / tableN;
            for (int i = 0; i <= tableN; ++i)
            {
                const float v = transfer (p, (float) (-R + dx * i));
                f[(size_t) i] = std::isfinite (v) ? juce::jlimit (-4.f, 4.f, v) : 0.f;
            }
            F[0] = 0.0;
            for (int i = 1; i <= tableN; ++i) F[(size_t) i] = F[(size_t) i - 1] + 0.5 * dx * (f[(size_t) i - 1] + f[(size_t) i]);
        }
        inline float eval (double x) const
        {
            const double pos = (juce::jlimit ((double) -R, (double) R, x) + R) / (2.0 * R) * tableN;
            const int i = juce::jmin (tableN - 1, (int) pos);
            const double fr = pos - i;
            return (float) (f[(size_t) i] + fr * (f[(size_t) i + 1] - f[(size_t) i]));
        }
        inline double integral (double x) const
        {
            if (x <= -R) return (x + R) * f[0];
            if (x >= R) return F[tableN] + (x - R) * f[tableN];
            const double dx = 2.0 * R / tableN, pos = (x + R) * (tableN / (2.0 * R));
            const int i = juce::jmin (tableN - 1, (int) pos);
            const double fr = pos - i, a = f[(size_t) i];
            return F[(size_t) i] + dx * fr * (a + 0.5 * (f[(size_t) i + 1] - a) * fr);   // (the exact integral of the linear piece)
        }
    };
    // first-order ADAA through a table
    struct Adaa
    {
        double x1 = 0.0, F1 = 0.0; bool primed = false; const Table* last = nullptr; uint32_t ser = 0;
        inline float process (const Table& t, double x)
        {
            const double F = t.integral (x);
            if (! primed || last != &t || ser != t.serial) { primed = true; last = &t; ser = t.serial; x1 = x; F1 = F; return t.eval (x); }
            const double dx = x - x1;
            const float y = std::abs (dx) < 1.0e-5 ? t.eval (0.5 * (x + x1)) : (float) ((F - F1) / dx);
            x1 = x; F1 = F;
            return std::isfinite (y) ? y : 0.f;
        }
    };

    // ------------------------------------------------------------------ halfband 2x up / down (FIR, linear phase, sample by sample)
    template <int N>
    struct Halfband
    {
        static const std::array<float, N>& coefs()
        {
            static const std::array<float, N> h = []
            {
                std::array<float, N> c {};
                const int m = (N - 1) / 2;
                for (int i = 0; i < N; ++i)
                {
                    const int k = i - m;
                    const double sinc = k == 0 ? 0.5 : std::sin (juce::MathConstants<double>::pi * k * 0.5) / (juce::MathConstants<double>::pi * k);
                    const double w = 0.42 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (N - 1)) + 0.08 * std::cos (4.0 * juce::MathConstants<double>::pi * i / (N - 1));
                    c[(size_t) i] = (float) (sinc * w);
                }
                return c;
            }();
            return h;
        }
        // polyphase: the zero-stuffed half of the work is skipped, the history is a doubled linear buffer (no modulo in the loops)
        static constexpr int L = (N + 1) / 2;
        static const std::array<float, L>& even() { static const auto e = [] { std::array<float, L> a {}; for (int j = 0; j < L; ++j) a[(size_t) j] = 2 * j < N ? coefs()[(size_t) (2 * j)] : 0.f; return a; }(); return e; }
        static const std::array<float, L>& odd()  { static const auto o = [] { std::array<float, L> a {}; for (int j = 0; j < L; ++j) a[(size_t) j] = 2 * j + 1 < N ? coefs()[(size_t) (2 * j + 1)] : 0.f; return a; }(); return o; }
        struct Hist
        {
            std::array<float, 2 * L> b {}; int i = 0;
            inline void push (float v) { i = i == 0 ? L - 1 : i - 1; b[(size_t) i] = v; b[(size_t) (i + L)] = v; }   // newest at b[i], older at b[i + j]
            inline float dot (const std::array<float, L>& h) const
            {
                // four partial sums: no long dependency chain (the compiler can vectorise it)
                const float* x = b.data() + i; float s0 = 0.f, s1 = 0.f, s2 = 0.f, s3 = 0.f; int j = 0;
                for (; j + 3 < L; j += 4) { s0 += h[(size_t) j] * x[j]; s1 += h[(size_t) j + 1] * x[j + 1]; s2 += h[(size_t) j + 2] * x[j + 2]; s3 += h[(size_t) j + 3] * x[j + 3]; }
                for (; j < L; ++j) s0 += h[(size_t) j] * x[j];
                return (s0 + s1) + (s2 + s3);
            }
        };
        Hist hx, ha, hb;
        inline void upsample (float x, float& a, float& b)              // one sample in, two out (gain kept)
        {
            hx.push (x);
            a = 2.f * hx.dot (even()); b = 2.f * hx.dot (odd());
        }
        inline float downsample (float a, float b)                       // two in, one out
        {
            ha.push (a); hb.push (b);
            return hb.dot (even()) + ha.dot (odd());
        }
        static constexpr int latency2x() { return N - 1; }                 // up + down, in samples of the higher rate
    };

    // ------------------------------------------------------------------ filters (pre / post)
    struct FilterSet { bool on = false; int type = FHigh, slope = 1; float cut = 120.f, reso = 0.f, drive = 0.f, mix = 1.f; doomsday::dsp::SvfCoefs c; float op = 0.1f, ladderG = 0.1f; };
    inline void cookFilter (FilterSet& fs, double rate, float cutMul = 1.f)
    {
        const float hz = juce::jlimit (20.f, (float) rate * 0.45f, fs.cut * cutMul);
        fs.c = doomsday::dsp::svfCoefs (hz, 1.f / (0.7f + fs.reso * fs.reso * 14.f), rate);
        fs.op = 1.f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) rate);
        fs.ladderG = std::tan (juce::MathConstants<float>::pi * hz / (float) rate);
    }
    struct FilterState
    {
        std::array<doomsday::dsp::Svf, 4> s {}; float op = 0.f; std::array<float, 4> z {};
        inline float process (const FilterSet& f, float x)
        {
            if (f.drive > 0.f) { const float g = 1.f + 8.f * f.drive; x = std::tanh (x * g) / std::tanh (g); }
            const float dry = x;
            float y = x;
            if (f.type == FLadder)                                           // 4 one-pole stages, resonance feedback with tanh
            {
                const float G = f.ladderG / (1.f + f.ladderG);
                float u = x - 3.9f * f.reso * std::tanh (z[3]);
                for (auto& zi : z) { const float v = (u - zi) * G; const float lp = v + zi; zi = lp + v; u = lp; }
                y = u;
            }
            else if (f.slope == 0 && (f.type == FLow || f.type == FHigh))     // 6 dB: one pole
            {
                op += f.op * (x - op);
                y = f.type == FLow ? op : x - op;
            }
            else
            {
                const int stages = f.slope <= 1 ? 1 : f.slope == 2 ? 2 : 4;
                for (int k = 0; k < stages; ++k)
                {
                    float l, b; s[(size_t) k].tick (y, f.c, l, b);
                    switch (f.type)
                    {
                        case FHigh:  y = y - f.c.k * b - l; break;
                        case FBand:  y = b; break;
                        case FNotch: y = y - f.c.k * b; break;
                        default:     y = l; break;
                    }
                }
            }
            return dry + (y - dry) * f.mix;
        }
    };

    // ------------------------------------------------------------------ one stage, cooked
    struct StageSettings
    {
        bool on = false; int loc = LocGlobal, order = 1, path = PathSerial, band = BandFull, type = Soft, morphType = Hard;
        float morph = 0.f, drive = 0.4f, inG = 1.f, outG = 1.f, mix = 1.f, bias = 0.f;
        int driveCurve = 1; bool autoGain = true;
        float g0 = 1.f, sm = 0.01f, rDc = 0.9996f;                        // cooked per block: the static gain, 2 ms smoothing, the 10 Hz DC blocker
        ShapeParams sp;
        const Table* tA = nullptr; const Table* tB = nullptr; const Table* tMod = nullptr;    // the curve, the morph target, SHAPE moved by the modulation
        float bits = 8.f, srr = 0.3f; int dither = 0;                      // BITCRUSH
        int os = 0;                                                        // 0 = engine 4x, 1 = 8x, 2 = 16x
        FilterSet pre, post;
        int modSrc = MsOwn; float modA = 20.f, modD = 200.f; const env::Shape* drawn = nullptr;
        float mDrive = 0.f, mBias = 0.f, mThr = 0.f, mFold = 0.f, mMix = 0.f, mOut = 0.f, mCut = 0.f;
        bool modOn() const { return std::abs (mDrive) + std::abs (mBias) + std::abs (mThr) + std::abs (mFold) + std::abs (mMix) + std::abs (mOut) + std::abs (mCut) > 1.0e-4f; }
    };
    inline float driveGain (float d, int curve)
    {
        d = juce::jlimit (0.f, 1.f, d);
        switch (curve)
        {
            case 0:  return 1.f + 31.f * d;                                      // LINEAR
            case 2:  return 1.f + 31.f * std::log10 (1.f + 9.f * d);            // LOGARITHMIC: fast at first
            default: return std::exp2 (5.f * d);                                // EXPONENTIAL: slow at first
        }
    }
    // what drives the modulation right now (per sample)
    struct ModCtx { float sinceHitMs = 1.0e9f, pitchNorm = 0.f, velocity = 1.f; };

    // ------------------------------------------------------------------ the running state of one stage on one channel
    struct StageState
    {
        FilterState pre, post;
        Adaa aA, aB, aM;
        Halfband<31> hb1; Halfband<33> hb2;
        std::array<float, 64> dly {}; int dI = 0;                          // the dry path delayed like the oversampled wet path
        float hold = 0.f, holdPh = 1.f; uint32_t rng = 0x1234567u;
        float dcX = 0.f, dcY = 0.f, inEnv = 0.f, pIn = 1.0e-6f, pOut = 1.0e-6f, ag = 1.f;
        int ctl = 0; FilterSet postMod; int seg = 0; int agTick = 0; float agTarget = 1.f; int modTick = 0; float mvHeld = 0.f, mvS = 0.f;
        float sG = -1.f, sMix = 0.f, sOut = 1.f, sBias = 0.f, sMorph = 0.f;   // smoothed (no zipper / clicks when settings move)
        uint64_t curveId = 0; float lastY = 0.f, lastY2 = 0.f, corr = 0.f;               // de-click: a curve / type switch glides over 3 ms
        void reset() { *this = StageState(); }

        inline float rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng >> 8) * (1.f / 8388608.f) - 1.f; }

        // the nonlinearity at the (possibly oversampled) rate
        inline float shape (const StageSettings& s, double x, float foldMix, float morph)
        {
            if (s.type == Bitcrush)
            {
                // SAMPLE RATE REDUCTION: really updates the value less often; BIT DEPTH (+ QUANT curve, DITHER)
                holdPh += s.srr;
                if (holdPh >= 1.f)
                {
                    holdPh -= 1.f;
                    float v = (float) juce::jlimit (-1.0, 1.0, x);
                    const float steps = std::exp2 (juce::jlimit (1.f, 24.f, s.bits)) * 0.5f;
                    if (s.dither == 1) v += (rnd() + rnd()) * 0.5f / steps;          // TPDF
                    else if (s.dither == 2) v += rnd() * 0.5f / steps;               // NOISE
                    if (s.sp.shape > 0.f) v = std::copysign (std::pow (std::abs (v), 1.f + 2.f * s.sp.shape), v);   // QUANTIZATION curve
                    hold = std::round (v * steps) / steps;
                }
                return hold;
            }
            float y = aA.process (*s.tA, x);
            if (foldMix > 0.f && s.tMod != nullptr) y += (aM.process (*s.tMod, x) - y) * foldMix;
            if (morph > 0.f && s.tB != nullptr) y += (aB.process (*s.tB, x) - y) * morph;   // MORPH: both algorithms run, the outputs blend
            return y;
        }

        inline float modValue (const StageSettings& s, const ModCtx& m)
        {
            switch (s.modSrc)
            {
                case MsDrawn:    return s.drawn != nullptr && m.sinceHitMs < s.modA + s.modD ? juce::jlimit (0.f, 1.f, s.drawn->eval (m.sinceHitMs / juce::jmax (1.f, s.modA + s.modD), seg)) : 0.f;
                case MsPitch:    return m.pitchNorm;
                case MsInput:    return juce::jlimit (0.f, 1.f, inEnv * 1.5f);
                case MsVelocity: return m.velocity;
                default:         { const float a = juce::jmax (0.05f, s.modA); return m.sinceHitMs < a ? m.sinceHitMs / a : std::exp (-(m.sinceHitMs - a) / juce::jmax (1.f, s.modD)); }
            }
        }
        // one sample through the whole stage
        inline float process (const StageSettings& s, float in, const ModCtx& m, double rate)
        {
            // ---- the modulation value 0..1
            // (the modulation value is read every 8 samples at the engine rate = 45 us; the 2 ms smoothing below joins the steps)
            if ((modTick++ & 7) == 0) mvHeld = s.modOn() ? modValue (s, m) : 0.f;
            mvS += 0.125f * (mvHeld - mvS);
            const float mv = mvS;
            const float aIn = std::abs (in);
            inEnv += (aIn > inEnv ? 0.05f : 0.0005f) * (aIn - inEnv);
            // ---- pre filter
            float x = s.pre.on ? pre.process (s.pre, in) : in;
            const float dry = in;
            // ---- drive / bias / threshold (as an input / output scale) / fold
            const float drive = juce::jlimit (0.f, 1.f, s.drive + s.mDrive * mv);
            const float thrScale = std::exp2 (-s.mThr * mv * 2.f);              // a lower threshold = the same curve driven harder
            const float gT = s.mDrive != 0.f || s.mThr != 0.f ? s.inG * driveGain (drive, s.driveCurve) * thrScale : s.g0;
            const float biasT = juce::jlimit (-1.f, 1.f, s.bias + s.mBias * mv) * 0.8f;
            const float sm = s.sm;                                                      // 2 ms
            if (sG < 0.f) { sG = gT; sBias = biasT; sMorph = s.morph; sMix = s.mix; sOut = s.outG; }
            sG += sm * (gT - sG); sBias += sm * (biasT - sBias); sMorph += sm * (juce::jlimit (0.f, 1.f, s.morph) - sMorph);
            const float g = sG, bias = sBias, morph = sMorph;
            const float foldMix = juce::jlimit (0.f, 1.f, std::abs (s.mFold) * mv);
            const double xd = (double) x * g + bias;
            float y;
            if (s.os == 0) y = shape (s, xd, foldMix, morph);
            else
            {
                float a, b; hb1.upsample ((float) xd, a, b);
                float ya, yb;
                if (s.os == 1) { ya = shape (s, a, foldMix, morph); yb = shape (s, b, foldMix, morph); }
                else
                {
                    float a1, a2, b1, b2; hb2.upsample (a, a1, a2); hb2.upsample (b, b1, b2);
                    // (one call per statement: the curve has memory (ADAA), the samples must go in time order)
                    const float s1 = shape (s, a1, foldMix, morph); const float s2 = shape (s, a2, foldMix, morph);
                    const float s3 = shape (s, b1, foldMix, morph); const float s4 = shape (s, b2, foldMix, morph);
                    ya = hb2.downsample (s1, s2); yb = hb2.downsample (s3, s4);
                }
                y = hb1.downsample (ya, yb);
            }
            // a new curve / type: the step between the old and the new output fades out over 3 ms (no click)
            {
                const uint64_t id = s.type == Bitcrush ? 0xB17ull : ((uint64_t) (uintptr_t) s.tA << 20) ^ (s.tA != nullptr ? s.tA->serial : 0u) ^ ((uint64_t) s.type << 56);
                if (id != curveId) { if (curveId != 0) corr += (2.f * lastY - lastY2) - y; curveId = id; }   // (the old curve's next value: extrapolated)
                lastY2 = lastY; lastY = y;
                if (corr != 0.f) { y += corr; corr *= std::exp (-1.f / (0.003f * (float) rate)); if (std::abs (corr) < 1.0e-6f) corr = 0.f; }
            }
            // the static offset a BIAS adds is taken away (+ a 10 Hz DC blocker for what the curve itself makes)
            if (bias != 0.f && s.type != Bitcrush)
            {
                float y0 = s.tA->eval (bias);
                if (morph > 0.f && s.tB != nullptr) y0 += (s.tB->eval (bias) - y0) * morph;
                y -= y0;
            }
            y /= thrScale;
            const float rDc = s.rDc;
            const float d = y - dcX + rDc * dcY; dcX = y; dcY = d; y = d;
            // ---- post filter (cutoff may move with the modulation)
            if (s.post.on)
            {
                if (s.mCut != 0.f)
                {
                    if ((ctl++ & 15) == 0) { postMod = s.post; cookFilter (postMod, rate, std::exp2 (s.mCut * mv * 5.f)); }
                    y = post.process (postMod, y);
                }
                else y = post.process (s.post, y);
            }
            // ---- AUTO GAIN: a slow RMS ratio (never per sample), clamped to +-18 dB
            sOut += sm * (s.outG - sOut);
            float out = y * sOut * (s.mOut != 0.f ? std::pow (10.f, s.mOut * mv * 12.f / 20.f) : 1.f);
            if (s.autoGain)
            {
                pIn += 0.0002f * (x * x - pIn); pOut += 0.0002f * (y * y - pOut);
                if ((agTick++ & 7) == 0) agTarget = juce::jlimit (0.125f, 8.f, std::sqrt ((pIn + 1.0e-9f) / (pOut + 1.0e-9f)));
                ag += 0.0005f * (agTarget - ag);
                out *= ag;
            }
            // the dry path waits for the oversampled wet path (MIX without comb filtering)
            const int lat = s.os == 0 ? 0 : s.os == 1 ? Halfband<31>::latency2x() / 2 : Halfband<31>::latency2x() / 2 + Halfband<33>::latency2x() / 4;
            dly[(size_t) dI] = dry;
            const float dryD = dly[(size_t) ((dI - lat + 64) % 64)];
            dI = (dI + 1) % 64;
            sMix += sm * (s.mix - sMix);
            const float mix = juce::jlimit (0.f, 1.f, sMix + s.mMix * mv);
            const float r = dryD + (out - dryD) * mix;
            return std::isfinite (r) ? r : 0.f;
        }
    };

    // ------------------------------------------------------------------ the static curve of a cooked stage (what TRANSFER CURVE draws):
    //  input -> drive -> bias -> the same tables the audio runs -> bias offset removed -> output gain -> mix (no modulation, filters, auto gain)
    inline float staticCurve (const StageSettings& s, float x)
    {
        const float g = s.inG * driveGain (s.drive, s.driveCurve), bias = juce::jlimit (-1.f, 1.f, s.bias) * 0.8f;
        float y;
        if (s.type == Bitcrush)
        {
            float v = juce::jlimit (-1.f, 1.f, x * g);
            const float steps = std::exp2 (juce::jlimit (1.f, 24.f, s.bits)) * 0.5f;
            if (s.sp.shape > 0.f) v = std::copysign (std::pow (std::abs (v), 1.f + 2.f * s.sp.shape), v);
            y = std::round (v * steps) / steps;
        }
        else if (s.tA != nullptr)
        {
            auto ev = [&] (float u) { float r = s.tA->eval (u); if (s.morph > 0.f && s.tB != nullptr) r += (s.tB->eval (u) - r) * juce::jlimit (0.f, 1.f, s.morph); return r; };
            y = ev (x * g + bias) - (bias != 0.f ? ev (bias) : 0.f);
        }
        else y = x;
        y *= s.outG;
        return x + (y - x) * s.mix;
    }

    // ------------------------------------------------------------------ all stages, cooked; the order per location
    struct DistBank
    {
        std::array<StageSettings, numStages> st {};
        std::array<std::array<int, numStages>, NumLocs> list {};          // stage indices per location, in ORDER
        std::array<int, NumLocs> count {};
        doomsday::dsp::SvfCoefs xLow, xHigh;                              // the multiband crossovers (LR4 = two SVF Butterworth)
        int bandSolo = 0, globalPos = 0;
        bool any = false;
    };

    // ------------------------------------------------------------------ one location (its chain) on one channel
    struct ChainState
    {
        std::array<StageState, numStages> s {};                          // indexed by stage number (only this location's stages run)
        // multiband: LR4 crossovers + the all-pass that keeps the low band in phase with mid + high
        doomsday::dsp::Svf l1a, l1b, h1a, h1b, l2a, l2b, h2a, h2b, la, lb, ha, hb;
        void reset() { for (auto& x : s) x.reset(); l1a = l1b = h1a = h1b = l2a = l2b = h2a = h2b = la = lb = ha = hb = {}; }

        inline float stageRun (const DistBank& b, int i, float x, const ModCtx& m, double rate) { return s[(size_t) i].process (b.st[(size_t) i], x, m, rate); }

        inline float process (const DistBank& b, int loc, float x, const ModCtx& m, double rate)
        {
            const int n = b.count[(size_t) loc];
            const auto& L = b.list[(size_t) loc];
            int k = 0;
            while (k < n)
            {
                const auto& first = b.st[(size_t) L[(size_t) k]];
                if (first.band != BandFull)                                // ---- MULTIBAND block: split, each band its stages, sum
                {
                    int e = k; while (e < n && b.st[(size_t) L[(size_t) e]].band != BandFull) ++e;
                    float low = l1b.lp (l1a.lp (x, b.xLow), b.xLow);
                    const float rest = h1b.hp (h1a.hp (x, b.xLow), b.xLow);
                    float mid = l2b.lp (l2a.lp (rest, b.xHigh), b.xHigh);
                    float high = h2b.hp (h2a.hp (rest, b.xHigh), b.xHigh);
                    // the low band gets the high crossover's all-pass (LP4 + HP4 at f2, = what mid + high went through) -> the bands sum flat
                    low = lb.lp (la.lp (low, b.xHigh), b.xHigh) + hb.hp (ha.hp (low, b.xHigh), b.xHigh);
                    for (int j = k; j < e; ++j)
                    {
                        const int si = L[(size_t) j];
                        const int bd = b.st[(size_t) si].band;
                        float& v = bd == BandLow ? low : bd == BandMid ? mid : high;
                        v = stageRun (b, si, v, m, rate);
                    }
                    if (b.bandSolo == 1) { mid = high = 0.f; } else if (b.bandSolo == 2) { low = high = 0.f; } else if (b.bandSolo == 3) { low = mid = 0.f; }
                    x = low + mid + high;
                    k = e;
                }
                else if (first.path != PathSerial)                         // ---- PARALLEL block: branches A / B / C, level compensated
                {
                    int e = k; while (e < n && b.st[(size_t) L[(size_t) e]].path != PathSerial && b.st[(size_t) L[(size_t) e]].band == BandFull) ++e;
                    float sum = 0.f; int branches = 0;
                    for (int p = PathA; p <= PathC; ++p)
                    {
                        float y = x; bool used = false;
                        for (int j = k; j < e; ++j) { const int si = L[(size_t) j]; if (b.st[(size_t) si].path == p) { y = stageRun (b, si, y, m, rate); used = true; } }
                        if (used) { sum += y; ++branches; }
                    }
                    x = branches > 0 ? sum / std::sqrt ((float) branches) : x;
                    k = e;
                }
                else { x = stageRun (b, L[(size_t) k], x, m, rate); ++k; }      // ---- SERIAL
            }
            return x;
        }
    };
}
