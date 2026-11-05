#pragma once
// =====================================================================
//  4GE module 07 - the EQ page's displays, computed on a background thread from the REAL DSP code (never on the message thread):
//   RESPONSE   - an impulse through the cooked EQ bands of the place (tone::EqState, the code the engine runs) -> dB per log bin;
//                the whole place and the selected band alone;
//   SPECTRUM   - the kick rendered with this place's module 07 processors bypassed (PRE) and active (POST), FFT (size chosen),
//                MID / L / R; the dominant frequency and its level in dBFS;
//   MULTIBAND  - the post render split by the same crossovers: the level of each band;
//   STEREO     - correlation of the post render (overall + per band: low / mid / high) and the side / mid ratio per band.
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/Engine.h"
#include <mutex>

namespace fge::gui
{
    class EqScope : private juce::Thread, private juce::AsyncUpdater
    {
    public:
        using Tables = std::array<dsp::TablePtr, 8>;
        struct Request { ParamFrame f; env::Bank bank; Tables tables; dsp::dist::DistData dist; int loc = 0, band = 0, fftOrder = 13, note = 31; };
        struct Result
        {
            std::vector<float> resp, bandResp, pre, post;       // dB per log bin (20 Hz .. 20 kHz)
            float domHz = 0.f, domDb = -120.f;
            std::array<float, 4> mbLevel {};                     // dBFS per multiband band (post)
            float corr = 1.f; std::array<float, 3> corrBand { 1, 1, 1 }, sideRatio {};
            int loc = -1, band = -1; bool valid = false;
        };
        static constexpr int bins = 240;
        static float binHz (int k) { return 20.f * std::pow (1000.f, (float) k / (float) (bins - 1)); }

        EqScope() : juce::Thread ("4GE eq scope") { startThread (juce::Thread::Priority::low); }
        ~EqScope() override { signalThreadShouldExit(); wake.signal(); stopThread (4000); }
        void ask (std::unique_ptr<Request> r) { std::lock_guard<std::mutex> l (lock); pending = std::move (r); wake.signal(); }
        Result result() const { std::lock_guard<std::mutex> l (lock); return shown; }
        std::function<void()> onReady;

        static std::vector<float> spectrumOf (const std::vector<float>& x, double rate, int order, bool window)
        {
            const int n = 1 << order;
            juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) n * 2, 0.f);
            const int m = juce::jmin (n, (int) x.size());
            for (int i = 0; i < m; ++i) buf[(size_t) i] = x[(size_t) i] * (window ? 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) m) : 1.f);
            fft.performFrequencyOnlyForwardTransform (buf.data());
            const float norm = window ? 4.f / (float) m : 1.f;
            std::vector<float> o ((size_t) bins);
            for (int k = 0; k < bins; ++k)
            {
                const float f0 = binHz (k) * 0.97f, f1 = binHz (k) * 1.03f;
                const int i0 = juce::jlimit (1, n / 2 - 1, (int) (f0 * n / rate)), i1 = juce::jlimit (i0, n / 2 - 1, (int) (f1 * n / rate));
                float e = 0.f; for (int i = i0; i <= i1; ++i) e = juce::jmax (e, buf[(size_t) i]);
                o[(size_t) k] = juce::Decibels::gainToDecibels (e * norm, -140.f);
            }
            return o;
        }
        static Result analyse (const Request& q)
        {
            namespace tn = dsp::tone;
            Result r; r.loc = q.loc; r.band = q.band;
            const double rate = 44100.0 * 4.0;
            // ---- RESPONSE: an impulse through the real band states of this place (both positions; EQ only = linear)
            {
                auto bank = std::make_unique<tn::Bank>();
                tn::cookBank (q.f, *bank, rate);
                auto run = [&] (bool onlySel)
                {
                    auto st = std::make_unique<tn::State>();
                    const int n = 1 << 15;
                    std::vector<float> y ((size_t) n);
                    for (int i = 0; i < n; ++i)
                    {
                        float l = i == 0 ? 1.f : 0.f, rr = l;
                        for (int pos = 0; pos < 2; ++pos)
                        {
                            const int c = bank->eqCount[(size_t) q.loc][(size_t) pos];
                            for (int k = 0; k < c; ++k)
                            {
                                const int b = bank->eqList[(size_t) q.loc][(size_t) pos][(size_t) k];
                                if (onlySel && b != q.band) continue;
                                auto e = bank->eq[(size_t) b]; e.stereo = tn::EsStereo; e.mix = bank->eq[(size_t) b].mix;
                                st->eq[(size_t) b].process (e, l, rr, rate);
                            }
                        }
                        y[(size_t) i] = l;
                    }
                    return spectrumOf (y, rate, 15, false);
                };
                // (states start smoothed at their targets: the first update sets them directly)
                r.resp = run (false); r.bandResp = run (true);
            }
            // ---- SPECTRUM pre / post: the kick with this place's module 07 off / on
            dsp::OscTables ot; for (size_t i = 0; i < q.tables.size(); ++i) ot.t[i] = q.tables[i].get();
            ParamFrame off = q.f;
            const int es = P::eq2On - P::eq1On, ds = P::deq2On - P::deq1On;
            for (int i = 0; i < tn::numEq; ++i) if ((int) off[P::eq1Loc + i * es] == q.loc) off[P::eq1On + i * es] = 0.f;
            for (int i = 0; i < tn::numDeq; ++i) if ((int) off[P::deq1Loc + i * ds] == q.loc) off[P::deq1On + i * ds] = 0.f;
            if ((int) off[P::mbLoc] == q.loc) off[P::mbOn] = 0.f;
            if ((int) off[P::spLoc] == q.loc) off[P::spOn] = 0.f;
            if (tn::stwLocs[juce::jlimit (0, 6, (int) off[P::stwLoc])] == q.loc) off[P::stwOn] = 0.f;
            const double secs = 0.5;
            auto pre = renderKick (off, q.note, 44100.0, secs, -1, 1.f, &q.bank, 0.0, -1.0, &ot, nullptr, &q.dist);
            auto post = renderKick (q.f, q.note, 44100.0, secs, -1, 1.f, &q.bank, 0.0, -1.0, &ot, nullptr, &q.dist);
            auto mono = [] (const juce::AudioBuffer<float>& b) { std::vector<float> v ((size_t) b.getNumSamples()); for (int i = 0; i < b.getNumSamples(); ++i) v[(size_t) i] = 0.5f * (b.getSample (0, i) + b.getSample (1, i)); return v; };
            const int order = juce::jlimit (11, 15, q.fftOrder);
            r.pre = spectrumOf (mono (pre), 44100.0, order, true); r.post = spectrumOf (mono (post), 44100.0, order, true);
            for (int k = 0; k < bins; ++k) if (r.post[(size_t) k] > r.domDb) { r.domDb = r.post[(size_t) k]; r.domHz = binHz (k); }
            // ---- MULTIBAND band levels + STEREO correlation (post)
            {
                auto bank = std::make_unique<tn::Bank>(); tn::cookBank (q.f, *bank, 44100.0);
                std::array<tn::XSet, 3> xs;
                const float x1 = q.f[P::mbX1], x2 = juce::jmax (q.f[P::mbX2], x1 * 1.25f), x3 = juce::jmax (q.f[P::mbX3], x2 * 1.25f);
                tn::cookX (xs[0], tn::XLr24, x1, 44100.0); tn::cookX (xs[1], tn::XLr24, x2, 44100.0); tn::cookX (xs[2], tn::XLr24, x3, 44100.0);
                tn::XState a, b, c; std::array<double, 4> e {};
                tn::XSet sl, sh; tn::cookX (sl, tn::XLr24, q.f[P::stwXLow], 44100.0); tn::cookX (sh, tn::XLr24, juce::jmax (q.f[P::stwXHigh], q.f[P::stwXLow] * 1.25f), 44100.0);
                std::array<tn::XState, 4> sx;
                double lr = 0, ll = 0, rr = 0; std::array<double, 3> blr {}, bll {}, brr {}, bm {}, bs {};
                for (int i = 0; i < post.getNumSamples(); ++i)
                {
                    const double L = post.getSample (0, i), R = post.getSample (1, i), m = 0.5 * (L + R);
                    double b0, rest, b1, rest2, b2, b3; a.split (xs[0], m, b0, rest); b.split (xs[1], rest, b1, rest2); c.split (xs[2], rest2, b2, b3);
                    e[0] += b0 * b0; e[1] += b1 * b1; e[2] += b2 * b2; e[3] += b3 * b3;
                    lr += L * R; ll += L * L; rr += R * R;
                    double l0, lrest, l1, l2, r0, rrest, r1, r2;
                    sx[0].split (sl, L, l0, lrest); sx[1].split (sh, lrest, l1, l2); sx[2].split (sl, R, r0, rrest); sx[3].split (sh, rrest, r1, r2);
                    const double lb[3] { l0, l1, l2 }, rb[3] { r0, r1, r2 };
                    for (int k = 0; k < 3; ++k)
                    {
                        blr[(size_t) k] += lb[k] * rb[k]; bll[(size_t) k] += lb[k] * lb[k]; brr[(size_t) k] += rb[k] * rb[k];
                        bm[(size_t) k] += 0.25 * (lb[k] + rb[k]) * (lb[k] + rb[k]); bs[(size_t) k] += 0.25 * (lb[k] - rb[k]) * (lb[k] - rb[k]);
                    }
                }
                const double n = juce::jmax (1, post.getNumSamples());
                for (int k = 0; k < 4; ++k) r.mbLevel[(size_t) k] = juce::Decibels::gainToDecibels ((float) std::sqrt (e[(size_t) k] / n), -120.f);
                r.corr = (float) (lr / std::sqrt (ll * rr + 1.0e-20));
                for (int k = 0; k < 3; ++k)
                {
                    r.corrBand[(size_t) k] = (float) (blr[(size_t) k] / std::sqrt (bll[(size_t) k] * brr[(size_t) k] + 1.0e-20));
                    r.sideRatio[(size_t) k] = (float) std::sqrt (bs[(size_t) k] / (bm[(size_t) k] + 1.0e-20));
                }
            }
            r.valid = true;
            return r;
        }

    private:
        void run() override
        {
            while (! threadShouldExit())
            {
                wake.wait (250);
                std::unique_ptr<Request> q;
                { std::lock_guard<std::mutex> l (lock); q = std::move (pending); }
                if (q == nullptr) continue;
                auto r = analyse (*q);
                { std::lock_guard<std::mutex> l (lock); if (pending != nullptr) continue; shown = std::move (r); }
                triggerAsyncUpdate();
            }
        }
        void handleAsyncUpdate() override { if (onReady) onReady(); }
        mutable std::mutex lock;
        std::unique_ptr<Request> pending;
        Result shown;
        juce::WaitableEvent wake;
    };
}
