#pragma once
// =====================================================================
//  4GE module 11 - STEREO & SPATIAL ENGINE (pure DSP: no GUI, no files, no allocation on the audio thread)
//   One UNIT per group (PUNCH / BODY / TAIL / SUB / TEXTURE / CUSTOM) and one on the MASTER bus, after that place's modules 05-08.
//   Signal order of a unit:
//     CHANNEL SWAP -> POLARITY (L / R / both) -> CHANNEL DELAY (L or R, fractional) -> M / S
//     -> LOW FREQUENCY MONO (LR4: the side keeps only the highs; the mid is untouched)
//     -> WIDTH algorithm (M/S WIDTH | CONTROLLED DECORRELATION | HAAS | DISABLED) with its MIX
//     -> M/S ENGINE (MID / SIDE level, M/S WIDTH, MID / SIDE SOLO) -> PAN (the mid, constant power) -> L / R
//     -> BALANCE (only turns the opposite channel down) -> OUTPUT GAIN.
//   WIDTH never raises the level (energy-compensated above 100 %); all gains are smoothed per sample (no clicks under modulation).
//   SUB MONO LOCK (global): the SUB group always leaves its unit mono, and the MASTER unit keeps everything below its crossover
//   (at least 120 Hz) mono - stronger than any WIDTH.
//   The ANALYZER (correlation, mid / side / L / R levels, vector-scope points) is filled by the processor for the GUI, lock free.
// =====================================================================
#include "../Tone/Tone.h"
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace fge::dsp::spatial
{
    enum Target { TPunch = 0, TBody, TTail, TSub, TTexture, TCustom, TMaster, NumTargets };
    enum Mode { MMs = 0, MDecor, MHaas, MOff };
    enum Pol { POff = 0, PLeft, PRight, PBoth };

    struct Settings
    {
        bool on = false, swap = false, msOn = false, midSolo = false, sideSolo = false, lowMono = false, forceMono = false;
        int mode = MMs, pol = POff, haasCh = 1;
        float width = 1.f, mix = 1.f, outG = 1.f, midG = 1.f, sideG = 1.f, msWidth = 1.f;
        float panL = 1.f, panR = 1.f, balL = 1.f, balR = 1.f;
        float delayL = 0.f, delayR = 0.f, haasSamples = 0.f;          // (samples at the engine rate)
        tone::XSet x;
    };
    struct Bank
    {
        std::array<Settings, NumTargets> t {};
        bool any = false;
    };

    // one unit's running state (the delay lines are allocated once, in the constructor)
    class Unit
    {
    public:
        static constexpr int delayN = 1 << 16;                           // 65536 samples: 40 ms + 10 ms even at 4 x 192 kHz
        Unit() : bufL ((size_t) delayN, 0.f), bufR ((size_t) delayN, 0.f) {}
        void reset()
        {
            std::fill (bufL.begin(), bufL.end(), 0.f); std::fill (bufR.begin(), bufR.end(), 0.f);
            w = 0; first = true; eIn = eOut = 0.0; comp = 1.0; sideX = midX = {}; for (auto& a : apL) a = {}; for (auto& a : apR) a = {};
        }
        inline void process (const Settings& s, float& l, float& r)
        {
            if (first) { cur = target (s); first = false; }
            // smoothing (5 ms at the engine rate is set by 'k' from the cook; the delay times move slower)
            const Smooth tg = target (s);
            cur.step (tg, kFast, kSlow);
            double L = s.swap ? r : l, R = s.swap ? l : r;
            if (s.pol == PLeft || s.pol == PBoth) L = -L;
            if (s.pol == PRight || s.pol == PBoth) R = -R;
            // CHANNEL DELAY + HAAS (one write, fractional reads)
            bufL[(size_t) w] = (float) L; bufR[(size_t) w] = (float) R;
            if (cur.dl > 0.01f) L = read (bufL, cur.dl);
            if (cur.dr > 0.01f) R = read (bufR, cur.dr);
            double m = 0.5 * (L + R), sd = 0.5 * (L - R);
            // LOW FREQUENCY MONO: the side loses everything below the crossover (LR4 high band); the mid stays untouched (the punch
            // keeps its exact shape). The decorrelation below is made only from the mid's highs, so it never widens the lows either.
            double mHigh = m;
            if (s.lowMono)
            {
                double lo, hi; sideX.split (s.x, sd, lo, hi); sd = hi;
                double ml, mh; midX.split (s.x, m, ml, mh); mHigh = mh;
            }
            // WIDTH algorithm (MIX = how much of it)
            double sw = sd;
            switch (s.mode)
            {
                case MMs: sw = sd * cur.width; break;
                case MDecor:
                {
                    // below 100 %: the side is turned down; above: a decorrelated side made from the mid's highs (pure side = mono safe)
                    const double extra = juce::jmax (0.0, (double) cur.width - 1.0);
                    sw = sd * juce::jmin (1.0, (double) cur.width);
                    double a = mHigh, b = mHigh;                                    // (the all-passes always run: no jump when the width moves)
                    for (int k = 0; k < 3; ++k) { a = apL[(size_t) k].tick (a, decoL[(size_t) k]); b = apR[(size_t) k].tick (b, decoR[(size_t) k]); }
                    sw += 0.5 * extra * (a - b);
                    break;
                }
                case MHaas:
                {
                    // the chosen channel arrives later (amount = WIDTH up to 100 %); creative - mono may suffer (the GUI says so)
                    const double amt = juce::jmin (1.0, (double) cur.width);
                    const double dL = s.haasCh == 0 ? read (bufL, cur.dl + cur.haas) : L, dR = s.haasCh == 1 ? read (bufR, cur.dr + cur.haas) : R;
                    const double hl = L + (dL - L) * amt, hr = R + (dR - R) * amt;
                    double hs = 0.5 * (hl - hr);
                    if (s.lowMono) { double lo, hi; haasX.split (s.x, hs, lo, hi); hs = hi; }
                    sw = hs;                                                         // (the mid stays: Haas only moves the image)
                    break;
                }
                default: break;
            }
            const double sdIn = sd;
            sd = sd + (sw - sd) * cur.mix;
            // level compensation: the width stage may never make it louder (mid^2 + side^2 measured before / after, ~30 ms)
            eIn += kSlow * (m * m + sdIn * sdIn - eIn); eOut += kSlow * (m * m + sd * sd - eOut);
            const double want = eOut > eIn && eOut > 1.0e-12 ? std::sqrt (eIn / eOut) : 1.0;
            comp += kFast * (want - comp);
            m *= comp; sd *= comp;
            // M/S ENGINE
            if (s.msOn)
            {
                m *= cur.midG; sd *= cur.sideG * cur.msWidth;
                if (s.midSolo && ! s.sideSolo) sd = 0.0;
                if (s.sideSolo && ! s.midSolo) m = 0.0;
            }
            // PAN = where the mid sits (constant power, 0 dB in the centre); BALANCE = the opposite channel down
            double outL = m * cur.panL + sd, outR = m * cur.panR - sd;
            if (s.forceMono) outL = outR = 0.5 * (outL + outR);
            outL *= cur.balL * cur.outG; outR *= cur.balR * cur.outG;
            w = (w + 1) & (delayN - 1);
            l = std::isfinite (outL) ? (float) outL : 0.f; r = std::isfinite (outR) ? (float) outR : 0.f;
            if (std::abs (l) < 1.0e-25f) l = 0.f;
            if (std::abs (r) < 1.0e-25f) r = 0.f;
        }
        float kFast = 0.01f, kSlow = 0.001f;                               // (setRate: 5 ms gains, 30 ms delay times / levels)
        std::array<double, 3> decoL { 0.62, -0.31, 0.47 }, decoR { -0.55, 0.36, -0.42 };
        // the engine rate: smoothing speeds + the decorrelation all-passes (different phase corners left / right, 150 Hz - 5 kHz)
        void setRate (double rate)
        {
            kFast = (float) (1.0 - std::exp (-1.0 / (0.005 * rate))); kSlow = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));
            auto co = [rate] (double hz) { const double t = std::tan (juce::MathConstants<double>::pi * juce::jmin (hz, 0.45 * rate) / rate); return (t - 1.0) / (t + 1.0); };
            const double hl[3] { 150.0, 900.0, 3800.0 }, hr[3] { 320.0, 1700.0, 5200.0 };
            for (int k = 0; k < 3; ++k) { decoL[(size_t) k] = co (hl[k]); decoR[(size_t) k] = co (hr[k]); }
        }
    private:
        struct Smooth
        {
            float width = 1.f, mix = 1.f, outG = 1.f, midG = 1.f, sideG = 1.f, msWidth = 1.f, panL = 1.f, panR = 1.f, balL = 1.f, balR = 1.f, dl = 0.f, dr = 0.f, haas = 0.f;
            void step (const Smooth& t, float k, float ks)
            {
                width += k * (t.width - width); mix += k * (t.mix - mix); outG += k * (t.outG - outG); midG += k * (t.midG - midG); sideG += k * (t.sideG - sideG);
                msWidth += k * (t.msWidth - msWidth); panL += k * (t.panL - panL); panR += k * (t.panR - panR); balL += k * (t.balL - balL); balR += k * (t.balR - balR);
                dl += ks * (t.dl - dl); dr += ks * (t.dr - dr); haas += ks * (t.haas - haas);
            }
        };
        static Smooth target (const Settings& s)
        {
            Smooth t;
            t.width = s.mode == MOff ? 1.f : s.width; t.mix = s.mix; t.outG = s.outG; t.midG = s.midG; t.sideG = s.sideG; t.msWidth = s.msWidth;
            t.panL = s.panL; t.panR = s.panR; t.balL = s.balL; t.balR = s.balR; t.dl = s.delayL; t.dr = s.delayR; t.haas = s.haasSamples;
            return t;
        }
        inline double read (const std::vector<float>& b, float d) const
        {
            const float p = (float) w - juce::jlimit (0.f, (float) (delayN - 4), d);
            const float pp = p < 0.f ? p + (float) delayN : p;
            const int i0 = (int) pp; const float f = pp - (float) i0;
            const float a = b[(size_t) (i0 & (delayN - 1))], c = b[(size_t) ((i0 + 1) & (delayN - 1))];
            return (double) (a + (c - a) * f);
        }
        struct Ap1 { double x1 = 0.0, y1 = 0.0; inline double tick (double x, double c) { const double y = c * x + x1 - c * y1; x1 = x; y1 = y; return y; } };
        std::vector<float> bufL, bufR;
        int w = 0; bool first = true;
        double eIn = 0.0, eOut = 0.0, comp = 1.0;
        Smooth cur;
        tone::XState sideX, midX, haasX;
        std::array<Ap1, 3> apL {}, apR {};
    };

    // ---- the analyzer the processor fills (output of the plugin) and the GUI reads - lock free
    struct Analyzer
    {
        static constexpr int scopeN = 1024;
        std::array<std::atomic<float>, scopeN> sx {}, sy {};             // vector scope: x = side, y = mid
        std::atomic<int> scopeW { 0 };
        std::atomic<float> corr { 1.f }, midRms { 0.f }, sideRms { 0.f }, lRms { 0.f }, rRms { 0.f };
        double eLL = 0, eRR = 0, eLR = 0, eM = 0, eS = 0; int cnt = 0, dec = 0;
        // audio thread: one block of the output
        void push (const float* l, const float* r, int n, double rate)
        {
            const int every = juce::jmax (1, (int) (rate / 22050.0));
            const double k = 1.0 - std::exp (-1.0 / (0.15 * rate));       // 150 ms averaging
            for (int i = 0; i < n; ++i)
            {
                const double L = l[i], R = r != nullptr ? r[i] : l[i], M = 0.5 * (L + R), S = 0.5 * (L - R);
                eLL += k * (L * L - eLL); eRR += k * (R * R - eRR); eLR += k * (L * R - eLR); eM += k * (M * M - eM); eS += k * (S * S - eS);
                if (++dec >= every)
                {
                    dec = 0; const int wi = scopeW.load (std::memory_order_relaxed);
                    sx[(size_t) wi].store ((float) S, std::memory_order_relaxed); sy[(size_t) wi].store ((float) M, std::memory_order_relaxed);
                    scopeW.store ((wi + 1) % scopeN, std::memory_order_release);
                }
            }
            const double den = std::sqrt (eLL * eRR);
            corr.store (den > 1.0e-12 ? (float) juce::jlimit (-1.0, 1.0, eLR / den) : 1.f, std::memory_order_relaxed);
            midRms.store ((float) std::sqrt (eM)); sideRms.store ((float) std::sqrt (eS)); lRms.store ((float) std::sqrt (eLL)); rRms.store ((float) std::sqrt (eRR));
        }
    };
}
